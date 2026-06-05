# maxurl

_max · Devices_

> Make HTTP requests

maxurl is a wrapper around libcurl that can perform HTTP requests. Use it to fetch and post web content.	For more information on curl please refer to the

 Curl Docs Page.

 To learn about and practice HTTP Requests, check out

 httpbin.org.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Start an HTTP request (dictionary or message) |
| in1 | Dictionary or Messages In |
| out0 | Output dictionary |
| out1 | Progress info (list) |

### Port details

**`in0` (Start an HTTP request (dictionary or message)):** Initialize an HTTP request using a dictionary for more advanced features or a simple message starting with get, post, put or delete.

**`out0` (Output dictionary):** The output dictionary contains the http response status, body, etc.

**`out1` (Progress info (list)):** Progress info list with the name of the output dictionary (or 'progress' if unnamed), download total, download now, upload total, upload now. Note that not all HTTP servers send the download total header.

## Arguments

- **thread count** (`int`) _(optional)_ — Possible number of concurrent requests
  If you would like maxurl to perform HTTP requests in sequence, then set the thread count to 1. If you would like to make multiple requests simultaneously, set the thread count to something greater than one. maxurl will then be able to execute that many requests concurrently. If you start maxurl with 3 threads and tell it to make 5 requests, 3 requests will be started immediately and 2 will be queued to start whenever any of the initial 3 requests finish.

## Messages

- `abort([response dictionary name: list])` — Abort maxurl requests.
  An abort message without any arguments will abort all running requests that this maxurl is performing. Optionally, a list of response dictionary names may be given to abort only those requests. To do the later, you will want to make your request with a specified response dictionary name. See the "response_dict" field of the dictionary message below.
- `abortall` — Abort all maxurl requests.
  Aborts all running requests and queued requests.
- `delete(url: list)` — Sends an HTTP DELETE request
  Sends an HTTP DELETE request to the specified URL.
- `dictionary(name: symbol)` — Dictionary input to set the HTTP request info
  Dictionary input to perform a more advanced HTTP request.
  key name
  value
  response_dict
  (string) sets the response dictionary name
  url
  (string) sets the url to which maxurl should make a request. (e.g. "http://cycling74.com" )
  http_method
  (string) one of four options: "get", "post", "put", or "delete"
  post_data
  (string) a url encoded string containing your post variables (e.g. "key1=val1&key2=val2")
  (dictionary) If post_data is another dictionary, maxurl will encode the dictionary as json data in the post body.
  filename_out
  (string) full path name of your output file. If this is set, the "body" key of the response dictionary will be empty.
  filename_in
  (string) full path name of your input filen. If this is set, and http_method is "put" or "post", maxurl will read this file as the body of your http request. Use this or the 'multiput_form' field to upload files to a server.
  useragent
  (string) the user agent name for your request. Use this to spoof other browsers. By default maxurl declares itself as Firefox on windows.
  timeout
  (number) set the timeout length in seconds. NOTE! This will terminate a running connection if it takes longer than the set timeout length.
  connect_timeout
  (number) set the timeout length in seconds for making a connection to a remote host.
  parse_type
  (string) one of either "none", "json" or "xml". Default is "none". If set to "json" or "xml", maxurl will respectively try to parse the response data into a dict from json or xml data.
  headers
  (array of strings) set the request headers directly (e.g ["Content-Type=text/plain; charset=UTF-8", "Server=httpbin"]).
  cookie
  (string) cookies are accepted and shared among requests in a maxurl object. However, you may set the request cookie directly here with something like 'name1=value1;name2=value2;'
  http_auth
  (string) set the http authentication to be sent with "username:password". By default, maxurl will attempt all authentication methods (basic, digest, gss negotiate, ntlm)
  multipart_form
  (array of dictionaries or dictionary or dictionaries) individual parts should have 2-3 keys, "name", "file", "content", or "contenttype". eg:
  [ {"name":"file-0", "file": "/full/path/to/filename.jpg"}, {"name": "file-1", "file": "/full/path/to/filename2.jpg"}, {"name": "filecount", "content": "2" } ] OR: { "part1" : {"name":"myfirstfile", "file": "/full/path/to/filename.jpg"}, "part2" : {"name": "anotherfile", "file": "/full/path/to/filename2.jpg"}, "part3" : {"name": "filecount", "content": "2" } }
  overwrite_response_dict
  (long) Either 0 or 1. If set to 1 (default) maxurl will allow you to make multiple requests that return data in the same response dictionary. If set to 0, maxurl will not allow you to queue your request if there already exists a running or queued request with the same response dictionary.
  overwrite_output_file
  (long) Either 0 or 1. By default this is set to 0 and maxurl will not overwite output files. If set to 1, you can force maxurl to overwrite an output file. If there is a running request that is writing data to that filename, it will be aborted.
  proxy
  (string) Set a proxy server.
  proxy_type
  (string) Either http (default), socks4, socks4a, or socks5
  proxy_auth
  (string) If your proxy requires a password set this to "username:password"
- `get(url: list)` — Sends an HTTP GET request
  Sends an HTTP GET request to the specified URL.
- `post(url: list)` — Sends an HTTP POST request
  Sends an HTTP POST request to the specified URL. Any pairs of symbols following the url will be sent as key-value pairs in the url encoded form.
- `put(url: list)` — Sends an HTTP PUT request
  Sends an HTTP PUT request to the specified URL. Any pairs of symbols following the url will be sent as key-value pairs in the url encoded form.
- `verbosity(: int)` — Sets the verbosity level of maxurl
  Determines the extent to which the maxurl object posts information about its processes to the Max console. Verbosity ranges from 0 - 6, with 0 disabling all console reporting, 1 being the default and most basic level of verbosity, and 6 being the most verbose.

## Attributes

- `@introduced` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — Dictionary or Messages In

### uploading

> One way to upload files is to send a POST or PUT request with the file as the data. To do this, set the 'filename_in' field on maxurl.

> Another way that is especially good for uploading multiple files at once is what's known as a multipart form. It can take a set of data and encode it in a format viable for HTTP transport.

> Click here to select a test file to upload. This time, we will set a "multipart_form" dictionary with "name" and "file" fields.

```
Example — [maxurl 1]
  fan-in:
    in0 ← [dict @embed 1]
    in0 ← [dict @embed 1]
  fan-out:
    out0 → [dict]:in0
    out1 → [unpack sym 0 0 0 0]:in0
```

### parsing

```
Example — [maxurl 2]
  fan-in:
    in0 ← [dict @embed 1] ← [button]    # This third example fetches real time bouy data from the Scripps Institute Station #46237 in the San Francisco Bay. That data is from an older system using a space delimited text format. We will need javascript to parse it into something meaningful.
    in0 ← [dict @embed 1] ← [button]    # We fetch the current weather conditions for San Francisco. Since we know the response body is in json format, we set "parse_type" to "json". This will parse the json text into a usable dictionary.
    in0 ← [dict @embed 1] ← [button]    # Same example, but this time the returned format is XML. In this case, we set the "parse_type" to "xml" in the request dict so that maxurl parses the returned data into in dictionary form for use inside of Max.
  fan-out:
    out0 → [dict]:in0
    out1 → [unpack sym 0 0 0 0]:in0
```

### XMLHttpRequest

> You may also make HTTP requests within javascript using the XMLHttpRequest object that is found in many web browsers. The Max implementation of XMLHttpRequest (aka XHR) uses maxurl in the background and has relative feature parity with the one found in browsers. Since there is no document object model (DOM) in Max, there are some features (FormData, Blob's, etc) that are currently senseless/useless to implement. We have also added a generic '_setRequestKey'' function to set all maxurl parameters that might not be available in the standard XHR object.

> A "get" request that uses javascript's internal JSON.parse to read single elements from an array in the returned data.

> Because it is currently not possible in Max to access a single dictionary element that is inside and array of dictionary elements using just the dict object, you will want to use the JSON.parse method inside of a js context. This "fetchweather" example here fetches weather data and then parses it using javascript.

### request dictionary options

> (string) sets the response dictionary name
>
> (string) sets the url to which maxurl should make a request. (e.g. "http://cycling74.com" )
>
> (string) one of four options: "get", "post", "put", or "delete"
>
> (string) a url encoded string containing your post variables (e.g. "key1=val1&key2=val2") (dictionary) If post_data is another dictionary, maxurl will encode the dictionary as json data in the post body.
>
> (string) full path name of your output file. If this is set, the "body" key of the response dictionary will be empty.
>
> (string) full path name of your input filen. If this is set, and http_method is "put" or "post", maxurl will read this file as the body of your http request. Use this or the 'multiput_form' field to upload files to a server.
>
> (string) the user agent name for your request. Use this to spoof other browsers. By default maxurl declares itself as Firefox on windows.
>
> (number) set the timeout length in seconds. NOTE! This will terminate a running connection if it takes longer than the set timeout length.
>
> (number) set the timeout length in seconds for making a connection to a remote host.
>
> (string) one of either "none", "json" or "xml". Default is "none". If set to "json" or "xml", maxurl will respectively try to parse the response data into a dict from json or xml data. WARNING: becuase max dictionaries cannot currently access objects inside of arrays, you will want to parse the dictionary inside of javascript if you need to do something with an array of JSON objects. See the "XMLHttpRequest" tab.
>
> (array of strings) set the request headers directly (e.g ["Content-Type=text/plain; charset=UTF-8", "Server=httpbin"]).
>
> (string) cookies are accepted and shared among requests in a maxurl object. However, you may set the request cookie directly here with something like 'name1=value1;name2=value2;'
>
> (string) set the http authentication to be sent with "username:password". By default, maxurl will attempt all authentication methods (basic, digest, gss negotiate, ntlm)
>
> (array of dictionaries or dictionary or dictionaries) individual parts should have 2-3 keys, "name", "file", "content", or "contenttype". eg: [ {"name":"file-0", "file": "/full/path/to/filename.jpg"}, {"name": "file-1", "file": "/full/path/to/filename2.jpg"}, {"name": "filecount", "content": "2" } ] OR: { "part1" : {"name":"myfirstfile", "file": "/full/path/to/filename.jpg"}, "part2" : {"name": "anotherfile", "file": "/full/path/to/filename2.jpg"}, "part3" : {"name": "filecount", "content": "2" } }
>
> (long) Either 0 or 1. If set to 1 (default) maxurl will allow you to make multiple requests that return data in the same response dictionary. If set to 0, maxurl will not allow you to queue your request if there already exists a running or queued request with the same response dictionary.
>
> (long) Either 0 or 1. By default this is set to 0 and maxurl will not overwite output files. If set to 1, you can force maxurl to overwrite an output file. If there is a running request that is writing data to that filename, it will be aborted.
>
> (string) Set a proxy server
>
> (string) Either http (default), socks4, socks4a, or socks5
>
> (string) If your proxy requires a password set this to "username:password"

> url

> http_method

> post_data

> filename_out

> filename_in

> useragent

> timeout

> connect_timeout

> parse_type

> headers

> cookie

> http_auth

> multipart_form

> overwrite_response_dict

> overwrite_output_file

> proxy

> proxy_type

> proxy_auth

### errors

> When dealing with HTTP, there are a number of ways errors could occur. There could be an error that causes the request not to fire or arrive at the server. This can occur if, for example, you make a request to a server that does not exist, or if the domain name cannot be resolved (eg, you mistyped the url), or if you are on a spotty internet connection. In these cases, maxurl will set an 'error' field in the output dictionary of maxurl. Another kind of error can happen when your request arrives at the server, but the server returns an HTTP status code such as 400, 404, or 500. While there are standards for what these codes should mean (see http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html), not all servers follow strict protocol. Whenever you make a request, you should also check if the status is acceptable (usually 200 or 304). When using maxurl, it is advised to follow the robustness principle of networking which states: be conservative in what you do, and liberal in what you accept.

```
Example — [maxurl]
  fan-in:
    in0 ← [dict @embed 1] ← [button]    # A "get" request on a server that exists but on a file that does not
    in0 ← [dict @embed 1]
    in0 ← [dict @embed 1] ← [button]    # A "get" request on a server name that doesn't exist
    in0 ← [dict @embed 1]
    in0 ← [message "abort"]    # abort all active requests
  fan-out:
    out0 → [dict]:in0
    out1 → [unpack sym 0 0 0 0]:in0
```

### header

```
Example — [maxurl]
  fan-in:
    in0 ← [dict @embed 1] ← [button]    # find out how many people liked this patch
    in0 ← [dict @embed 1] ← [button]    # sends a POST command to function which runs in Parse Cloud Code (parse.com) to increment a value
  fan-out:
    out0 → [route dictionary]:in0
```

### queued

> Each request sets a corresponding "response_dict" (res1, res2, res3), set in the request dictionary

```
Example — [maxurl]
  fan-in:
    in0 ← [dict @embed 1] ← [t b b b] ← [button]    # We will attempt to download multiple files. If maxurl is instantiated without any threads (as is the case below), it will queue the requests and execute them one after the other
    in0 ← [dict @embed 1] ← [t b b b] ← [button]    # We will attempt to download multiple files. If maxurl is instantiated without any threads (as is the case below), it will queue the requests and execute them one after the other
    in0 ← [dict @embed 1] ← [t b b b] ← [button]    # We will attempt to download multiple files. If maxurl is instantiated without any threads (as is the case below), it will queue the requests and execute them one after the other
  fan-out:
    out0 → [print @popup 1]:in0
    out1 → [unpack sym 0 0 0 0]:in0
```

### advanced

```
Example — [maxurl]
  fan-in:
    in0 ← [dict @embed 1] ← [button]    # post data to a site that reflects back the posted data
    in0 ← [dict @embed 1] ← [button]    # get the cycling home page file
    in0 ← [dict @embed 1] ← [button]    # get the c74 home page and save it to your desktop
    in0 ← [dict @embed 1] ← [button]    # same as above, except the post_data is sent as json, and the response 'body' is parsed as a json
  fan-out:
    out0 → [dict]:in0    # The output from maxurl comes as a dictionary. Double-click to view its content.
    out1 → [unpack sym 0 0 0 0]:in0
```

### threaded

> You can think of each maxurl instance as a browser. Each thread within maxurl is like a browser tab. The threads will all share cache and session data. If you need to make multiple simultaneous http requests that require login data of any kind, you will most likely want to use one maxurl with multiple threads/queues.

```
Example — [maxurl 3]  We tell maxurl to start with 3 threads. This allows it to execute 3 concurrent requests simultaneously. We can start maxurl with up to 32 threads.
  fan-in:
    in0 ← [message "abort firefox"]    # abort 'firefox' request
    in0 ← [message "abort"]
    in0 ← [message "abort googles"]    # abort all running requests / abort 'googles' request
    in0 ← [message "abortall"]    # abort all running AND queued requests
    in0 ← [dict @embed 1] ← [button]    # Download two large files at once: Mozilla Firefox and Google Chrome
    in0 ← [dict @embed 1] ← [button]    # For each, we declare a request in dictionary form. With each request, we specify a "response_dict" key. For this example, one is named "firefox" and the other "googles". maxurl will place the output dictionary in the respective dictionary names (ie, "firefox" and "googles"). / Download two large files at once: Mozilla Firefox and Google Chrome
  fan-out:
    out0 → [dict]:in0
    out1 → [route firefox googles]:in0    # Progress for each is also prefixed with the dictionary name as a symbol: "firefox" and "googles".
```

### basic

```
Example — [maxurl]
  fan-in:
    in0 ← [message "get https://cycling74.com/ ~/Desktop/cycling_home.html"]    # get the c74 home page and save it to your desktop.
    in0 ← [message "post http://httpbin.org/post someint 23 somefloat 3.045 somestring blah"]    # post data to a site that reflects back the posted data.
    in0 ← [message "get https://cycling74.com/"]    # get the cycling home page html and save in 'body' field of outlet dictionary.
  fan-out:
    out0 → [dict]:in0    # The output from maxurl comes as a dictionary. Double-click to view its content.
    out1 → [unpack sym 0 0 0 0]:in0
```

## See also

`jit.net.recv`, `jit.net.send`, `jit.uldl`, `jweb`, `udpreceive`, `udpsend`
