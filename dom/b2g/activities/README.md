# WebActivity API

The **WebActivity API** provides a way for applications to delegate an activity to another application. An *activity* is something a user wants to do: pick an image, play a video, etc. Apps can declare themselves as an activity hander, for example, Gallery App and installed 3rd-party photo-manage Apps can all be candidates when users request to pick photos. Usually such list of choices are provided to users.

The `WebActivity` interface exposes to both *Window* and *Worker* scope, in the window scope, creation of WebActivity is allowed when the requester application is visible (in the foreground), in the worker scope, creation of WebActivity is allowed only when handling a notification click event (TODO: this restriction is not yet implemented).

## Starting an activity

To initiate an activity, apps can first create a `WebActivity` object with proper `WebActivityOptions`, for example:

```javascript
var options = { name: "pick", data: { type: "image/jpeg" } };
var activity = new WebActivity(options);
```

In this example, we are trying to initiate a "pick" activity, and the handlers should support providing at least jpeg images.


<dl>
<b>WebActivityOptions</b>
    <dt>name</dt>
    <dd>A string represents the name of the activity request.</dd>
    <dt>data</dt>
    <dd>An object represents data that will be sent to activity handler, also refers to the filter of activity handlers.</dd>
</dl>

Then, start the activity by `start()` method, if an activity handler (i.e. Gallery App) is launched and the request is handled successfully, the promise will be resolved with the picking result.

```
activity.start().then(
  rv => {
    console.log("Results passed back from activity handler:")
    console.log(rv);
  },  
  err => {
    console.log(err);
  }
);
```

Please note that one WebActivity instance starts once, duplicate calls of start() is rejected.

## Cancel an activity

To cancel a started activity, use `cancel()`, and the pending promise of `start()` will also be rejected.

```javascript
activity.cancel();
```

## Handle an activity

Once the user has picked an activity handler (or picked by System App secretly), `SystemMessageEvent` with name *activity* will be dispatched to the event handler on its **service worker**, unlike other system messages, `SystemMessageData` has a property `WebActivityRequestHandler`, which provides methods to resolve or reject the pending promise of `activity.start()`.

Please note that per spec of ServiceWorkerGlobalScope[1], the activity handler is not persisted across the termination/restart cycle of worker. See [ServiceWorkerGlobalScope](https://developer.mozilla.org/en-US/docs/Web/API/ServiceWorkerGlobalScope) for more details.

>[1] Developers should keep in mind that the ServiceWorker state is not persisted across the termination/restart cycle, so each event handler should assume it's being invoked with a bare, default global state.

When handling `systemmessage` event of name *activity*, handler can decide whether to launch the application window to perform user interaction, by using ServiceWorker API, `Clients.openWindow()` (TODO: enable openWindow on gonk platform). Or can pass back results with `WebActivityRequestHandler.postResult()` directly. Suggest using `postMessage()` method to communicate between main scripts and service worker script.

Use `WebActivityRequestHandler.postError()` to send back an error message if something goes wrong.

```javascript
self.onsystemmessage = e => {
  try {
    let handler = e.data.webActivityRequestHandler();
    console.log("activity payload:");
    console.log(handler.source);
    let fakeResult = { images: ["a.jpeg", "b.png"], type: "favorite" };
    handler.postResult(fakeResult);
  } catch (err) {
    console.log(err);
  }
};

```

<dl>
<b>WebActivityRequestHandler</b>
    <dt>source</dt>
    <dd>A <code>WebActivityOptions</code> object contains the information about the current activity. Set by the app who starts the activity.</dd>
    <dt>postResult()</dt>
    <dd>Send back response and resolve the promise of app that starts the activity.</dd>
    <dt>postError()</dt>
    <dd>Send back error message and reject the promise of app that starts the activity.</dd>
</dl>

### Open its window with Clients.openWindow
TBD

## Register an App as an activity handler

App can register itself as an activity handler to handle one or more activities. Besides declaring activity related information in the app manifest, app has to set up the service worker script in its manifest first.

### Set up service worker registration

```javascript
"serviceworker": {
  "script_url": "script_url",
  "options": {
    "scope": "scope_of_sw",
    "update_via_cache": "value_of_update_via_cache"
  }
},
```
The `options` object is optional, as defined in [ServiceWorkerContainer.register()#Syntax](https://developer.mozilla.org/en-US/docs/Web/API/ServiceWorkerContainer/register#Syntax), for example:

```javascript
"serviceworker": {
  "script_url": "sw.js"
},
```

In the most case the above example is good enough.

### Set up activity handler registration

In general, please reference  [Registering an App as an activity handler](https://developer.mozilla.org/en-US/docs/Archive/B2G_OS/API/Web_Activities#Registering_an_App_as_an_activity_handler)



#### New attributes and updates for activity handler description

~~<code>**href**</code>~~
<br>No need to specify the href anymore.

~~<code>**disposition**</code>~~
<br>No need to specify the disposition anymore.

<code>**filter**</code> |Optional
<br>Same as before.

<code>**returnValue**</code> |Optional
<br>Same as before.

<code>**allowedOrigins**</code> |Optional
<br>An array of origins. This is for handler to limit its usage to specific users, will filter out callers who's origin is not in the list. For example, setting `"allowedOrigins": ["http://testapp1.localhost", "http://testapp2.localhost"]`, only allows requests made from "http://testapp1.localhost" or "http://testapp2.localhost".
