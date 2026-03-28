// pugl_mac_metal.m - Pugl Metal backend for macOS (outside the Pugl submodule)
// SPDX-License-Identifier: ISC

#include "internal.h"
#include "mac.h"
#include "stub.h"
#include "pugl_mac_metal.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>

// --- PuglMetalView ---
//
// A plain NSView (NOT layer-hosting) whose drawRect: drives the Pugl expose
// event.  A CAMetalLayer is attached as a sublayer so Metal rendering
// composites on top without interfering with AppKit's display machinery.

@interface PuglMetalView : NSView
@end

@implementation PuglMetalView {
@public
  PuglView        *puglview;
  PuglMetalContext  metalCtx;
}

- (id)initWithFrame:(NSRect)frame
{
  self = [super initWithFrame:frame];
  if (self) {
    // Enable layer backing so we can attach a Metal sublayer.
    // We do NOT override makeBackingLayer or wantsUpdateLayer, so AppKit
    // continues to call drawRect: normally via setNeedsDisplay/displayIfNeeded.
    self.wantsLayer = YES;

    metalCtx.device = MTLCreateSystemDefaultDevice();
    if (!metalCtx.device) {
      [self release];
      return nil;
    }

    metalCtx.commandQueue = [metalCtx.device newCommandQueue];

    metalCtx.metalLayer = [[CAMetalLayer layer] retain];
    metalCtx.metalLayer.device          = metalCtx.device;
    metalCtx.metalLayer.pixelFormat     = MTLPixelFormatBGRA8Unorm;
    metalCtx.metalLayer.framebufferOnly = YES;
    metalCtx.metalLayer.frame           = self.bounds;
    metalCtx.metalLayer.contentsScale   = self.window.backingScaleFactor ?: 2.0;
    metalCtx.metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;

    [self.layer addSublayer:metalCtx.metalLayer];
  }
  return self;
}

- (void)dealloc
{
  [metalCtx.metalLayer removeFromSuperlayer];
  [metalCtx.metalLayer release];
  [metalCtx.commandQueue release];
  [metalCtx.device release];

  metalCtx.metalLayer   = nil;
  metalCtx.commandQueue = nil;
  metalCtx.device       = nil;

  [super dealloc];
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldSize
{
  PuglWrapperView *wrapper = (PuglWrapperView *)[self superview];

  [super resizeWithOldSuperviewSize:oldSize];
  [wrapper setReshaped];
}

- (void)setFrameSize:(NSSize)newSize
{
  [super setFrameSize:newSize];
  metalCtx.metalLayer.frame = self.bounds;
}

- (void)viewDidMoveToSuperview
{
  [super viewDidMoveToSuperview];
  PuglWrapperView *wrapper = (PuglWrapperView *)[self superview];
  if (wrapper) {
    // Ensure the wrapper dispatches PUGL_CONFIGURE before the first
    // PUGL_EXPOSE.  NSOpenGLView triggers this via its reshape method,
    // but a plain layer-backed NSView does not.
    [wrapper setReshaped];
  }
}

- (void)drawRect:(NSRect)rect
{
  if (!puglview) {
    return;
  }

  PuglWrapperView *wrapper = (PuglWrapperView *)[self superview];

  [wrapper dispatchExpose:rect];
}

- (void)viewDidChangeBackingProperties
{
  [super viewDidChangeBackingProperties];
  if (self.window) {
    metalCtx.metalLayer.contentsScale = self.window.backingScaleFactor;
  }
}

@end

// --- Pugl backend callbacks ---

static PuglStatus
puglMacMetalCreate(PuglView *view)
{
  PuglInternals  *impl     = view->impl;
  PuglMetalView  *drawView = [PuglMetalView alloc];

  drawView->puglview = view;
  drawView = [drawView initWithFrame:[impl->wrapperView bounds]];
  if (!drawView) {
    return PUGL_CREATE_CONTEXT_FAILED;
  }

  if (view->hints[PUGL_RESIZABLE]) {
    [drawView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  } else {
    [drawView setAutoresizingMask:NSViewNotSizable];
  }

  impl->drawView = drawView;
  return PUGL_SUCCESS;
}

static void
puglMacMetalDestroy(PuglView *view)
{
  PuglMetalView *drawView = (PuglMetalView *)view->impl->drawView;

  // Prevent drawRect: from dispatching events during teardown.
  drawView->puglview = NULL;

  // Disconnect from AppKit's display system BEFORE releasing the view,
  // so no pending CATransaction can trigger rendering during teardown.
  [drawView setNeedsDisplay:NO];
  [drawView->metalCtx.metalLayer removeFromSuperlayer];
  [drawView removeFromSuperview];

  // Force Core Animation to commit the removal immediately.  Without this,
  // the layer tree changes are batched and the old CAMetalLayer may still
  // be accessed during the next CATransaction commit (e.g. if the host
  // immediately re-creates the plugin).
  [CATransaction flush];

  // Release the view.  Metal objects are freed in -dealloc, which fires
  // once AppKit drops its last reference (safe even if a CATransaction
  // is still retaining the view).
  [drawView release];

  view->impl->drawView = nil;
}

static PuglStatus
puglMacMetalEnter(PuglView *view, const PuglExposeEvent *expose)
{
  PuglMetalView *drawView = (PuglMetalView *)view->impl->drawView;
  if (!drawView) {
    return PUGL_FAILURE;
  }

  if (expose) {
    PuglMetalContext *ctx = &drawView->metalCtx;

    CGSize size = [drawView convertSizeToBacking:drawView.bounds.size];
    if (size.width <= 0 || size.height <= 0) {
      return PUGL_FAILURE;
    }

    ctx->metalLayer.drawableSize = size;

    ctx->currentDrawable = [ctx->metalLayer nextDrawable];
    if (!ctx->currentDrawable) {
      return PUGL_FAILURE;
    }

    ctx->commandBuffer = [ctx->commandQueue commandBuffer];

    ctx->renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    ctx->renderPassDescriptor.colorAttachments[0].texture     = ctx->currentDrawable.texture;
    ctx->renderPassDescriptor.colorAttachments[0].loadAction  = MTLLoadActionClear;
    ctx->renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    ctx->renderPassDescriptor.colorAttachments[0].clearColor  =
        MTLClearColorMake(0.12, 0.12, 0.12, 1.0);

    ctx->renderEncoder =
        [ctx->commandBuffer renderCommandEncoderWithDescriptor:ctx->renderPassDescriptor];
  }

  return PUGL_SUCCESS;
}

static PuglStatus
puglMacMetalLeave(PuglView *view, const PuglExposeEvent *expose)
{
  PuglMetalView *drawView = (PuglMetalView *)view->impl->drawView;
  if (!drawView) {
    return PUGL_FAILURE;
  }

  if (expose) {
    PuglMetalContext *ctx = &drawView->metalCtx;

    [ctx->renderEncoder endEncoding];
    [ctx->commandBuffer presentDrawable:ctx->currentDrawable];
    [ctx->commandBuffer commit];

    ctx->renderEncoder        = nil;
    ctx->commandBuffer        = nil;
    ctx->currentDrawable      = nil;
    ctx->renderPassDescriptor = nil;
  }

  return PUGL_SUCCESS;
}

static void *
puglMacMetalGetContext(PuglView *view)
{
  PuglMetalView *drawView = (PuglMetalView *)view->impl->drawView;
  if (!drawView) {
    return NULL;
  }
  return &drawView->metalCtx;
}

// --- Public API ---

const PuglBackend *
puglMetalBackend(void)
{
  static const PuglBackend backend = {puglStubConfigure,
                                      puglMacMetalCreate,
                                      puglMacMetalDestroy,
                                      puglMacMetalEnter,
                                      puglMacMetalLeave,
                                      puglMacMetalGetContext};
  return &backend;
}

// puglEnterContext / puglLeaveContext are defined in the GL backend file
// (mac_gl.m) which we no longer compile on macOS, so provide them here.

PuglStatus
puglEnterContext(PuglView *view)
{
  return view->backend->enter(view, NULL);
}

PuglStatus
puglLeaveContext(PuglView *view)
{
  return view->backend->leave(view, NULL);
}
