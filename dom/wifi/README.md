# Wi-Fi API

Wi-Fi API allows applications to control wifi hardware, such as turning on/off wifi, scanning access points, and establishing wifi connection.
For detail API description, please check into `dom/webidl/WifiManager.webidl`.

### Wi-Fi network configuration
The *WifiNetwork* contains the necessary information used to make wifi connection, or also used as event data or scan result.
```javascript
WifiNetwork {
  readonly attribute DOMString ssid;
  readonly attribute long mode;
  readonly attribute long frequency;
  readonly attribute DOMString security;
  readonly attribute boolean wpsSupported;
  readonly attribute boolean known;
  readonly attribute boolean connected;
  readonly attribute boolean hidden;
  readonly attribute boolean hasInternet;
  readonly attribute boolean captivePortalDetected;

  attribute DOMString? bssid;
  attribute DOMString? signalStrength;
  attribute long? relSignalStrength;
  attribute DOMString? psk;
  attribute DOMString? wep;
  attribute long? keyIndex;
  attribute boolean? scanSsid;
  attribute DOMString? identity;
  attribute DOMString? password;
  attribute DOMString? authAlg;
  attribute DOMString? phase1;
  attribute DOMString? phase2;
  attribute DOMString? eap;
  attribute DOMString? pin;
  attribute boolean? dontConnect;
  attribute DOMString? serverCertificate;
  attribute DOMString? subjectMatch;
  attribute DOMString? userCertificate;
  attribute long? simIndex;
  attribute DOMString? wapiPsk;
  attribute DOMString? pskType;
  attribute DOMString? wapiAsCertificate;
  attribute DOMString? wapiUserCertificate;
};
```

### Enable wifi
```javascript
navigator.b2g.wifiManager.setWifiEnabled(true);
```

### Disable wifi
```javascript
navigator.b2g.wifiManager.setWifiEnabled(false);
```

### Scan and receive results
Trigger wifi scan and there would be a list of *WifiNetwork* returned.
```javascript
navigator.b2g.wifiManager.getNetworks();
```
A example to register event callback to receive periodic scan results.
The returned scanResult object would be a list of *WifiNetwork*.
```javascript
navigator.b2g.wifiManager.onscanresult = (r) => {
  if (r.scanResult) {
    for (let network of r.scanResult) {
      console.log("Get scan result: " + network.ssid);
    }
  } else {
    console.log(`No scan result`);
  }
};
```

### Connect to open network
```javascript
var net = {
    ssid: "KaiOS",
    security: "OPEN",
};
navigator.b2g.wifiManager.associate(new window.WifiNetwork(net));
```

### Connect to WPA/WPA2-PSK network
```javascript
var net = {
    ssid: "KaiOS",
    security: "WPA-PSK",
    psk: "password",
};
navigator.b2g.wifiManager.associate(new window.WifiNetwork(net));
```

### Connect to WPA3-Personal (SAE) network
```javascript
var net = {
    ssid: "KaiOS",
    security: "SAE",
    psk: "password",
};
navigator.b2g.wifiManager.associate(new window.WifiNetwork(net));
```

### Connect to WEP network
```javascript
var net = {
    ssid: "KaiOS",
    security: "WEP",
    wep: "12345",
    keyIndex: 1,
};
navigator.b2g.wifiManager.associate(new window.WifiNetwork(net));
```

### Connect to EAP-SIM/AKA/AKA' network
```javascript
var net = {
    ssid: "KaiOS",
    security: "WPA-EAP",
    eap: "SIM" // "AKA", "AKA'"
};
navigator.b2g.wifiManager.associate(new window.WifiNetwork(net));
```

### Connect to EAP-PEAP/TTLS network
```javascript
var net = {
    ssid: "KaiOS",
    security: "WPA-EAP",
    eap: "PEAP",        // "TTLS"
    phase2: "MSCHAPV2", // "PAP", "GTC", "MSCHAP", ...
    identity: "demo@kaiostech.com",
    password: "password",
    serverCertificate: "ca",
};
navigator.b2g.wifiManager.associate(new window.WifiNetwork(net));
```

### Connect to EAP-TLS network
```javascript
var net = {
    ssid: "KaiOS",
    security: "WPA-EAP",
    eap: "TLS",
    identity: "demo",
    serverCertificate: "ca",
    userCertificate: "user",
};
navigator.b2g.wifiManager.associate(new window.WifiNetwork(net));
```

### Connect to WPS network
For WPS push button.
```javascript
var wps = { method: "pbc" };
navigator.b2g.wifiManager.wps(wps);
```
For WPS display pin code.
```javascript
var wps = { method: "display" };
navigator.b2g.wifiManager.wps(wps);
```

### Get current wifi connection status and detail information
```javascript
navigator.b2g.wifiManager.connection;
navigator.b2g.wifiManager.connectionInformation;
```

### Disconnect from network
The network with matched ssid and security type would be disconnected/removed.
```javascript
var net = {
    ssid: "KaiOS",
    security: "WPA-PSK", // "OPEN", "WPA-EAP", "WEP"
};
navigator.b2g.wifiManager.forget(new window.WifiNetwork(net));
```
