# Alarm API

Alarm API allows applications to schedule notification at a specific time in the future. For example, some applications like alarm-clock, calendar or auto-update might need to utilize the Alarm API to trigger particular device behaviors at specified time points.

Alarms persist after reboot. When bootup, all expired alarms are fired immediately. When an alarm is fired, it is dispatched to applications through the System Message API, so applications which want to react to alarms have to register themselves to the alarm messages.

Alarm API exposes to both *Window* and *Worker* scope.

## Get all alarms
```javascript
navigator.b2g.alarmManager.getAll().then(
  (list) => console.log(list), // Array of alarm objects.
  (err) => console.log("getAll err: " + err)
);
```

## Add an alarm
```javascript
options =
  {
    "date": new Date(Date.now() + 10*60*1000),
    "data": {"note": "take a nap"},
    "ignoreTimezone": false
  };

navigator.b2g.alarmManager.add(options).then(
  (id) => console.log("add id: " + id),
  (err) => console.log("add err: " + err)
);
```

## Remove an alarm
```javascript
navigator.b2g.alarmManager.remove(7);
```

## Handle alarm
Applications need to subscribe system message of type "alarm" to receive alarm messages. For more details, please see [Subscribe system messages](../messages/README.md)

### Subscribe alarm
#### Use from main scripts
```javascript
navigator.serviceWorker.register("sw.js").then(registration => {
  registration.systemMessageManager.subscribe("alarm").then(
    rv => {
      console.log('Successfully subscribe system messages of name "alarm".');
    },
    error => {
      console.log("Fail to subscribe system message, error: " + error);
    }
  );
});
```

### Receive alarm
```javascript
self.onsystemmessage = (e) => {
  console.log("receive systemmessage event on sw.js!");
  console.log(e.data.json()); // The alarm object
};
```
