/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum WifiWPSMethod {
  "pbc",
  "pin",
  "display",
  "cancel"
};

enum ConnectionStatus {
  "connecting",
  "authenticating",
  "associated",
  "connected",
  "disconnected",
  "wps-timedout",
  "wps-failed",
  "wps-overlapped",
  "dhcpfailed",
  "authenticationfailed",
  "associationreject"
};

dictionary WifiWPSInfo {
  WifiWPSMethod method;
  DOMString? pin;
  DOMString? bssid;
};

[GenerateInit]
dictionary NetworkProperties {
  DOMString ssid;
  long mode;
  long frequency;
  DOMString security;
  boolean wpsSupported;
  boolean known;
  boolean connected;
  boolean hidden;
  boolean hasInternet;
  boolean captivePortalDetected;
  DOMString bssid;
  DOMString signalStrength;
  long relSignalStrength;
  DOMString psk;
  DOMString wep;
  long keyIndex;
  boolean scanSsid;
  DOMString identity;
  DOMString password;
  DOMString authAlg;
  DOMString phase1;
  DOMString phase2;
  DOMString eap;
  DOMString pin;
  boolean dontConnect;
  DOMString serverCertificate;
  DOMString subjectMatch;
  DOMString userCertificate;
  long simIndex;
  DOMString wapiPsk;
  DOMString pskType;
  DOMString wapiAsCertificate;
  DOMString wapiUserCertificate;
};

[JSImplementation="@mozilla.org/wifinetwork;1",
 Func="B2G::HasWifiManagerSupport",
 Exposed=Window]
interface WifiNetwork {
  constructor(optional NetworkProperties networkPropertiesDict = {});

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

[JSImplementation="@mozilla.org/wificonnection;1",
 ChromeOnly,
 Exposed=Window]
interface WifiConnection {
  readonly attribute ConnectionStatus status;
  readonly attribute WifiNetwork? network;
};

[JSImplementation="@mozilla.org/wificonnectioninfo;1",
 ChromeOnly,
 Exposed=Window]
interface WifiConnectionInfo {
  readonly attribute short signalStrength;
  readonly attribute short relSignalStrength;
  readonly attribute long linkSpeed;
  readonly attribute DOMString? ipAddress;
};

dictionary IPConfiguration {
  boolean enabled;
  DOMString ipaddr;
  DOMString proxy;
  short maskLength;
  DOMString gateway;
  DOMString dns1;
  DOMString dns2;
};

[JSImplementation="@mozilla.org/wifimanager;1",
 Func="B2G::HasWifiManagerSupport",
 Exposed=Window]
interface WifiManager : EventTarget {
  /**
   * Turn on/off wifi functionality.
   * @param enable true for enable, false for disable.
   * onsuccess: Wifi enable/disable successfully, including no status change.
   * onerror: Wifi enable/disable failed or prohibited.
   */
  DOMRequest setWifiEnabled(boolean enabled);

  /**
   * Returns the list of currently available networks.
   * onsuccess: We have obtained the current list of networks. request.value
   *            is an object whose property names are SSIDs and values are
   *            network objects.
   * onerror: We were unable to obtain a list of property names.
   */
  DOMRequest getNetworks();

  /**
   * Returns the list of networks known to the system that will be
   * automatically connected to if they're in range.
   * onsuccess: request.value is an object whose property names are
   *            SSIDs and values are network objects.
   * onerror: We were unable to obtain a list of known networks.
   */
  DOMRequest getKnownNetworks();

  /**
   * Takes one of the networks returned from getNetworks and tries to
   * connect to it.
   * @param network A network object with information about the network,
   *                such as the SSID, key management desired, etc.
   * onsuccess: We have started attempting to associate with the network.
   *            request.value is true.
   * onerror: We were unable to select the network. This most likely means a
   *          configuration error.
   */
  DOMRequest associate(WifiNetwork network);

  /**
   * Given a network, removes it from the list of networks that we'll
   * automatically connect to. In order to re-connect to the network, it is
   * necessary to call associate on it.
   * @param network A network object with the SSID of the network to remove.
   * onsuccess: We have removed this network. If we were previously
   *            connected to it, we have started reconnecting to the next
   *            network in the list.
   * onerror: We were unable to remove the network.
   */
  DOMRequest forget(WifiNetwork network);

  /**
   * Wi-Fi Protected Setup functionality.
   * @param detail WPS detail which has 'method' and 'pin' field.
   *               The possible method field values are:
   *                 - pbc: The Push Button Configuration.
   *                 - pin: The PIN configuration.
   *                 - cancel: Request to cancel WPS in progress.
   *               If method field is 'pin', 'pin' field can exist and has
   *               a PIN number.
   *               If method field is 'pin', 'bssid' field can exist and has
   *               a opposite BSSID.
   * onsuccess: We have successfully started/canceled wps.
   * onerror: We have failed to start/cancel wps.
   */
  DOMRequest wps(optional WifiWPSInfo detail = {});

  /**
   * Turn on/off wifi power saving mode.
   * @param enabled true or false.
   * onsuccess: We have successfully turn on/off wifi power saving mode.
   * onerror: We have failed to turn on/off wifi power saving mode.
   */
  DOMRequest setPowerSavingMode(boolean enabled);

  /**
   * Given a network, configure using static IP instead of running DHCP
   * @param network A network object with the SSID of the network to set static ip.
   * @param info info should have following field:
   *        - enabled True to enable static IP, false to use DHCP
   *        - ipaddr configured static IP address
   *        - proxy configured proxy server address
   *        - maskLength configured mask length
   *        - gateway configured gateway address
   *        - dns1 configured first DNS server address
   *        - dns2 configured seconf DNS server address
   * onsuccess: We have successfully configure the static ip mode.
   * onerror: We have failed to configure the static ip mode.
   */
  DOMRequest setStaticIpMode(WifiNetwork network, optional IPConfiguration info = {});

  /**
   * Given a network, configure http proxy when using wifi.
   * @param network A network object with the SSID of the network to set http proxy.
   * @param info info should have following field:
   *        - httpProxyHost ip address of http proxy.
   *        - httpProxyPort port of http proxy, set 0 to use default port 8080.
   *        set info to null to clear http proxy.
   * onsuccess: We have successfully configure http proxy.
   * onerror: We have failed to configure http proxy.
   */
  DOMRequest setHttpProxy(WifiNetwork network, any info);

  /**
   * Import a certificate file, only support CA certificate now.
   * @param certBlob A Blob object containing raw data of certificate to be imported.
   *                 Supported format: binary/base64 encoded DER certificates.
   *                                   (.der/.crt/.cer/.pem)
   *                 Cause error if importing certificates already imported.
   * @param certPassword Password of certificate.
   * @param certNickname User assigned nickname for imported certificate.
   *                     Nickname must be unique, it causes error on redundant nickname.
   * onsuccess: We have successfully imported certificate. request.result is an
   *            object, containing information of imported CA:
   *            {
   *              nickname:  Nickname of imported CA, String.
   *              usage:     Supported usage of imported CA, Array of String,
   *                         includes: "ServerCert".
   *            }
   * onerror: We have failed to import certificate.
   */
  DOMRequest importCert(Blob certBlob,
                        DOMString certPassword,
                        DOMString certNickname);

  /**
   * Get list of imported WIFI certificates.
   * onsuccess: We have successfully gotten imported certificate list.
   *            request.result is an object using nickname as key, array of usage
   *            string as value.
   *            request.result[USAGE] = [CA_NICKNAME1, CA_NICKNAME2, ...]
   *            USAGE string includes: "ServerCert".
   * onerror: We have failed to list certificate.
   */
  DOMRequest getImportedCerts();

  /**
   * Delete an imported certificate.
   * @param certNickname Nickname of imported to be deleted.
   * onsuccess: We have successfully deleted certificate.
   * onerror: We have failed to delete certificate.
   */
  DOMRequest deleteCert(DOMString certNickname);

  /**
   * Add or update a Passpoint configuration, and save configuration in storage.
   * @param config A passpoint configuration object contains homeSp and credential.
   *        - homeSp: contains fqdn, friendlyName, roamingConsortiumOis, etc.
   *        - credential: contains realm, imsi, eapType, etc.
   * onsuccess: We have added the configuration successfully.
   *            request.result is a string of FQDN to identify the configuration.
   * onerror: We have failed to add passpoint configuration.
   */
  DOMRequest setPasspointConfig(optional PasspointConfiguration config = {});

  /**
   * Get the list of installed Passpoint configurations added by setPasspointConfig().
   * onsuccess: We have successfully gotten the installed configuration list.
   *            request.result is an array of PasspointConfiguration objects.
   * onerror: We have failed to get configuration list.
   */
  DOMRequest getPasspointConfigs();

  /**
   * Remove the Passpoint configuration identified by its FQDN.
   * @param fqdn The FQDN of the Passpoint configuration added by
   *             setPasspointConfig().
   * onsuccess: We have successfully remove the configuration.
   * onerror: We have failed to remove the configuration..
   */
  DOMRequest removePasspointConfig(DOMString fqdn);

  /**
   * Attribute to enable or disable open network notification.
   */
  attribute boolean openNetworkNotificationEnabled;

  /**
   * Returns whether or not wifi is currently enabled.
   */
  readonly attribute boolean enabled;

  /**
   * Returns whether or not openNetworkNotify is currently enabled.
   */
  readonly attribute boolean openNetworkNotify;

  /**
   * Returns whether or not wifi has internet.
   */
  readonly attribute boolean hasInternet;

  /**
   * Returns the last wifi scan result.
   */
  readonly attribute any scanResult;

  /**
   * Returns the MAC address of the wifi adapter.
   */
  readonly attribute DOMString macAddress;

  /**
   * An non-null object containing the following information:
   *  - status ("disconnected", "connecting", "associated", "connected")
   *  - network
   *
   *  Note that the object returned is read only. Any changes required must
   *  be done by calling other APIs.
   */
  readonly attribute WifiConnection connection;

  /**
   * A connectionInformation object with the same information found in an
   * WifiConnectionInfoEvent (but without the network).
   * If we are not currently connected to a network, this will be null.
   */
  readonly attribute WifiConnectionInfo? connectionInformation;

  /**
   * Capabilities of Wifi.
   */
  readonly attribute WifiCapabilities? capabilities;

  /**
   * State notification listeners. These all take an
   * WifiStatusChangeEvent with the new status and a network (which may be
   * null).
   *
   * The possible statuses are:
   *   - connecting: Fires when we start the process of connecting to a
   *                 network.
   *   - associated: Fires when we have connected to an access point but do
   *                 not yet have an IP address.
   *   - connected: Fires once we are fully connected to an access point and
   *                can access the internet.
   *   - disconnected: Fires when we either fail to connect to an access
   *                   point (transition: associated -> disconnected) or
   *                   when we were connected to a network but have
   *                   disconnected for any reason (transition: connected ->
   *                   disconnected).
   */
  attribute EventHandler onstatuschange;

  /**
   * An event listener that is called with information about the signal
   * strength and link speed every 5 seconds.
   */
  attribute EventHandler onconnectioninfoupdate;

  /**
   * These two events fire when the wifi system is brought online or taken
   * offline.
   */
  attribute EventHandler onenabled;
  attribute EventHandler ondisabled;

  /**
   * An event listener that is called with information about the number
   * of wifi stations connected to wifi hotspot every 5 seconds.
   */
  attribute EventHandler onstationinfoupdate;

  /**
   * An event listener that is called with notification about the open
   * wifi network available.
   */
  attribute EventHandler onopennetwork;

  /**
   * An event listener that is called with wifi scan result.
   */
  attribute EventHandler onscanresult;

  /**
   * An event listener that is called with notification about the current
   * wifi network has internet or not.
   */
  attribute EventHandler onwifihasinternet;

  /**
   * An event listener that is called with notification about the current
   * wifi network is captive portal or not.
   */
  attribute EventHandler oncaptiveportallogin;
};
