## **Node for Max API**

Version 9.1.4-rev.0

cycling74.com
## **Contents**

## **Node for Max API**

Reference for the API exposed as "max-api" when running NodeJS applications using the `[node.script]` object. This API can be required using

```
const maxAPI = require("max-api");
```

## **Enums**

|**Item**|**Description**|
|---|---|
|MAX_ENV|Enum options for the value Node For Max will set on<br>process.env.MAX_ENV|
|MESSAGE_TYPES|Predefined generic MaxFunctionSelector types|
|POST_LEVELS|Log Levels used in maxAPI.post|

## **Functions**

|**Item**|**Description**|
|---|---|
|addHandler|Register a single handler|

|addHandlers|Register handlers|
|---|---|
|getDict|Get the value of a dict object|
|outlet|Outlet any values|
|outletBang|Outlet a Bang|
|post|Post to the Max console. Setting the last argument to a value of<br>maxAPI.POST_LEVELS allows control of the log level|
|removeHandler|Remove a single handler|
|removeHandlers|Remove handlers|
|setDict|Set the value of a dict object|
|updateDict|Partially update the value of a dict object at a given path|

## **Type Aliases**

**Item Description** Anything JSONArray JSONObject JSONPrimitive JSONValue MaxFunctionHandler MaxFunctionSelector

## **function addHandler**

Register a single handler

```
exportfunction addHandler(selector: MaxFunctionSelector, handler:
MaxFunctionHandler): void;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|selector|MaxFunctionSelector||
|handler|MaxFunctionHandler||

## **function addHandlers**

Register handlers

```
exportfunction addHandlers(handlers: Record<MaxFunctionSelector,
MaxFunctionHandler>): void;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|handlers|Record<MaxFunctionSelector,MaxFunctionHandler>||

## **type Anything**

```
exporttype Anything = string | number | Array<string | number> |
JSONObject | JSONArray;
```

## **function getDict**

Get the value of a dict object

```
exportfunction getDict(id: string): Promise<JSONObject>;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|id|string||
|Return Value|Promise<JSONObject>||

## **type JSONArray**

```
exporttype JSONArray = Array<JSONValue>;
```

## **type JSONObject**

```
exporttype JSONObject = { [key: string]: JSONValue | undefined };
```

## **type JSONPrimitive**

```
exporttype JSONPrimitive = string | number | boolean | null;
```

## **type JSONValue**

```
exporttype JSONValue = JSONPrimitive | JSONArray | JSONObject;
```

## **enum MAX_ENV**

Enum options for the value Node For Max will set on process.env.MAX_ENV

## **Members**

|**Member**|**Value**|**Description**|
|---|---|---|
|MAX|`"max"`|node.script running from within Max|
|MAX_FOR_LIVE|`"maxforlive"`|node.script running from within Max For Live|
|STANDALONE|`"max:standalone"`|node.script running from within a standalone<br>application|

## **type MaxFunctionHandler**

```
exporttype MaxFunctionHandler = (...args: any[]) => any;
```

## **type MaxFunctionSelector**

```
exporttype MaxFunctionSelector = MESSAGE_TYPES | string;
```

## **enum MESSAGE_TYPES**

Predefined generic MaxFunctionSelector types

## **Members**

|**Member**|**Value**|**Description**|
|---|---|---|
|ALL|`"all"`|Generic Type for *all* kinds of messages|
|BANG|`"bang"`|Bang message type|
|DICT|`"dict"`|Dictionary message type|
|LIST|`"list"`|List message type|
|NUMBER|`"number"`|Number message type|

## **function outlet**

Outlet any values

```
exportfunction outlet(...args: JSONValue[]): Promise<void>;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|args|JSONValue[]||
|Return Value|Promise<void>||

## **function outletBang**

Outlet a Bang

```
exportfunction outletBang(): Promise<void>;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|Return Value|Promise<void>||

## **enum POST_LEVELS**

Log Levels used in maxAPI.post

## **Members**

|**Member**|**Value**|**Description**|
|---|---|---|
|ERROR|`"error"`|error level messages|
|INFO|`"info"`|info level messages|
|WARN|`"warn"`|warn level messages|

## **function post**

Post to the Max console. Setting the last argument to a value of maxAPI.POST_LEVELS allows control of the log level

```
exportfunction post(...args: Array<Anything | POST_LEVELS>):
Promise<void>;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|args|Array<Anything|POST_LEVELS>||
|Return Value|Promise<void>||

## **function removeHandler**

Remove a single handler

```
exportfunction removeHandler(selector: MaxFunctionSelector, handler:
MaxFunctionHandler): void;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|selector|MaxFunctionSelector||
|handler|MaxFunctionHandler||

## **function removeHandlers**

Remove handlers

```
exportfunction removeHandlers(selector: MaxFunctionSelector): void;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|selector|MaxFunctionSelector||

## **function setDict**

Set the value of a dict object

```
exportfunction setDict(id: string, dict: JSONObject): Promise<JSONObject>;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|id|string||
|dict|JSONObject||
|Return Value|Promise<JSONObject>||

## **function updateDict**

Partially update the value of a dict object at a given path

```
exportfunction updateDict(id: string, updatePath: string, updateValue:
JSONValue): Promise<JSONObject>;
```

|**Name**|**Type**|**Description**|
|---|---|---|
|id|string||
|updatePath|string||
|updateValue|JSONValue||
|Return Value|Promise<JSONObject>||
