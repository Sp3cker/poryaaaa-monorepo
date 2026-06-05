# jweb

__

> Web browser

The jweb object uses the cross-platform Chromium Embedded Framework (CEF) to host web pages within a Max UI object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Control Input |
| out0 | Status and Javascript Output |

### Port details

**`out0` (Status and Javascript Output):** When loading a page, various status messages about the current state of the page loading are sent out the outlet. Messages include onloadstart, onloadend (followed by a status code), url with the currently loaded URL, and title with the current page title. Depending on the site, the url and title output may happen more than once. Once a page is loaded, the outlet will send out any message sent to the page via the MaxAPI.outlet function in JavaScript.

## Messages

- `anything` — Send messages
  You can send any message to the JavaScript of the contained website. In order to be able to receive messages make sure to setup the JavaScript as described in the Web Browser and jweb guide.
- `back` — Load the previously visited page
  The back message reloads the previous page of data, functioning like the 'Back' button in a conventional web browser.
- `executejavascript(script: symbol)` — Execute JavaScript
  Using executejavascript you can inject and run a JavaScript code snippet in the currently displayed web page. The provided code will be inserted and executed in a new script tag.
- `forward` — Move forward in visited page history
  When a sequence of multiple pages has been loaded, and the object has received the back message, the word forward will advance to the following page in the object's page history. forward functions like the 'Forward' button in a conventional web browser.
- `gotohistory(index: int)` — Navigate to a page in the browser history
  If the history attribute has been set to a named dictionary, gotohistory followed by an index will navigate to the page with that index in the dictionary. The most recently visited page is stored at index 0.
- `mute(mute-state: int)` — Mute audio output
  mute 1 will mute the audio output of the page; mute 0 will unmute
- `read(url: symbol)` — Read URL or file
  The word read, followed by a symbol that specifies a URL or a file pathname, will read the webpage or file and attempt to render its contents. Upon successful load of a page, two messages are sent from the object's outlet: url, followed the final URL which was loaded based on the provided argument; and title, followed by the title of the loaded page as a symbol.
- `readfile([file path: symbol])` — Read a file
  The word readfile, followed by a symbol that specifies a file pathname, will read the file and attempt to render its contents. The word readfile with no argument opens a file dialog to choose a file. Upon successful load of a page, two messages are sent from the object's outlet: url, followed the final URL which was loaded based on the provided argument; and title, followed by the title of the loaded page as a symbol.
- `reload([ignorecache: int])` — Refresh the current page
  The reload message causes the browser to refresh the current page, functioning like the 'Reload' button in a conventional web browser. The word reload followed by a non-zero argument will ignore the locally cached file.

## GUI behaviors

- `(mouse)` — Operate the loaded page
  Use the mouse (and keyboard) to interact with the page displayed by jweb in a locked patcher.

## Attributes

- `@label` (symbol)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `url` — seen as: `url http://www.cycling74.com`, `url http://www.google.com`

## Help patcher examples

### transparency

> The rendermode attribute includes an options for transparent background rendering. This allows you to use jweb for creating user interface elements that can work on complex backgrounds.

```
Example #1 — [jweb]  in a locked patcher, play with jweb-rendered dials
  fan-in:
    in0 ← [message "readfile jwt_01.html"]
    in0 ← [attrui @rendermode]
```

```
Example #2 — [jweb]
  fan-in:
    in0 ← [message "readfile jwt_01.html"]
```

```
Example #3 — [jweb]
  fan-in:
    in0 ← [message "readfile jwt_01.html"]
```

```
Example #4 — [jweb]
  fan-in:
    in0 ← [message "readfile jwt_01.html"]
```

```
Example #5 — [jweb]
  fan-in:
    in0 ← [p animator] ← [toggle] ← [loadmess 1]    # p animator emits: "value $1" | "bgfillcolor $1 $2 $3"
    in0 ← [message "readfile jwt_01.html"]
  fan-out:
    out0 → [message ""]:in1
```

Attributes demonstrated: `@rendermode`

### basic

```
Example — [jweb]
  fan-in:
    in0 ← [message "back"]
    in0 ← [message "forward"]    # navigation
    in0 ← [message "reload"]    # reload current page
    in0 ← [message "url http://www.cycling74.com"]
    in0 ← [message "readfile"]    # open local html files
    in0 ← [message "url http://www.google.com"]    # internet addresses
  fan-out:
    out0 → [route url resource title]:in0
```

## See also

`web_browser`, `jweb~`, `js`, `maxurl`
