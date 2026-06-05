# jit.gl.light

_jit · Jitter OpenGL_

> Place a light source in a 3D scene

Contains the properties needed to define a light source in OpenGL. These include light type (directional, point and spot), light color, attenuation, and spot angle and falloff. In addition a position (for point and spot) and orientation (for directional and spot) can be defined for a virtual light in 3D space.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | dumpout |

## Messages

- `sendoutput` — Send the shadow output texture a message

## Attributes

- `@basic` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — dumpout

### shadows

> Shadow properties are controlled via jit.gl.light attributes and jit.gl.material attributes

```
Example — [jit.gl.light @type spot @axes 1 @shadows 1 @enable 0 @position -2 2 2 @lookat 0 0 0]
  fan-in:
    in0 ← [attrui @shadowquality]
    in0 ← [attrui @enable]
    in0 ← [attrui @position]
    in0 ← [attrui @rotatexyz]
    in0 ← [attrui @shadows]    # enable shadow-casting for this light
    in0 ← [attrui @shadowblur]    # shadow properties
    in0 ← [attrui @shadowrange]
```

Attributes demonstrated: `@enable`, `@position`, `@rotatexyz`, `@shadowblur`, `@shadowquality`, `@shadowrange`, `@shadows`

### spotlight

> Several parameters control the spotlight shape and intensity

```
Example — [jit.gl.light @type spot @axes 1 @enable 0 @position 2 2 2 @lookat 0 0 0]
  fan-in:
    in0 ← [attrui @atten_const]
    in0 ← [attrui @atten_linear]
    in0 ← [attrui @atten_quad]
    in0 ← [attrui @enable]
    in0 ← [attrui @diffuse]
    in0 ← [attrui @rotatexyz]
    in0 ← [attrui @type]
    in0 ← [attrui @spot_angle]
    in0 ← [attrui @spot_falloff]
```

Attributes demonstrated: `@atten_const`, `@atten_linear`, `@atten_quad`, `@diffuse`, `@enable`, `@rotatexyz`, `@spot_angle`, `@spot_falloff`, `@type`

### basic

```
Example — [jit.gl.light lit-ctx]
  fan-in:
    in0 ← [attrui @rotatexyz]
    in0 ← [attrui @specular]
    in0 ← [attrui @ambient]
    in0 ← [attrui @type]
    in0 ← [attrui @position]
    in0 ← [attrui @enable]
    in0 ← [attrui @axes]
    in0 ← [attrui @diffuse]
```

Attributes demonstrated: `@ambient`, `@axes`, `@diffuse`, `@enable`, `@gizmos`, `@position`, `@rotatexyz`, `@specular`, `@type`

## See also

`jit.gl.render`, `jit.gl.sketch`, `jit.gl.material`
