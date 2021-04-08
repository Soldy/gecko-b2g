/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetworkUtils_h
#define NetworkUtils_h

#include "nsString.h"
#include "mozilla/dom/NetworkOptionsBinding.h"
#include "mozilla/dom/network/NetUtils.h"
#include "mozilla/FileUtils.h"
#include "nsTArray.h"
#include "NetIdManager.h"

class NetworkParams;
class CommandChain;

class CommandCallback {
 public:
  typedef void (*CallbackType)(CommandChain*, bool,
                               mozilla::dom::NetworkResultOptions& aResult);

  typedef void (*CallbackWrapperType)(
      CallbackType aOriginalCallback, CommandChain*, bool,
      mozilla::dom::NetworkResultOptions& aResult);

  CommandCallback() : mCallback(nullptr), mCallbackWrapper(nullptr) {}

  explicit CommandCallback(CallbackType aCallback)
      : mCallback(aCallback), mCallbackWrapper(nullptr) {}

  CommandCallback(CallbackWrapperType aCallbackWrapper,
                  CommandCallback aOriginalCallback)
      : mCallback(aOriginalCallback.mCallback),
        mCallbackWrapper(aCallbackWrapper) {}

  void operator()(CommandChain* aChain, bool aError,
                  mozilla::dom::NetworkResultOptions& aResult) {
    if (mCallbackWrapper) {
      return mCallbackWrapper(mCallback, aChain, aError, aResult);
    }
    if (mCallback) {
      return mCallback(aChain, aError, aResult);
    }
  }

 private:
  CallbackType mCallback;
  CallbackWrapperType mCallbackWrapper;
};

typedef void (*CommandFunc)(CommandChain*, CommandCallback,
                            mozilla::dom::NetworkResultOptions& aResult);
typedef void (*MessageCallback)(mozilla::dom::NetworkResultOptions& aResult);
typedef void (*ErrorCallback)(NetworkParams& aOptions,
                              mozilla::dom::NetworkResultOptions& aResult);

class NetworkParams {
 public:
  NetworkParams() {}

  explicit NetworkParams(const mozilla::dom::NetworkCommandOptions& aOther) {
#define COPY_SEQUENCE_FIELD(prop, type)                \
  if (aOther.prop.WasPassed()) {                       \
    mozilla::dom::Sequence<type> const& currentValue = \
        aOther.prop.InternalValue();                   \
    uint32_t length = currentValue.Length();           \
    for (uint32_t idx = 0; idx < length; idx++) {      \
      prop.AppendElement(currentValue[idx]);           \
    }                                                  \
  }

#define COPY_OPT_STRING_FIELD(prop, defaultValue)    \
  if (aOther.prop.WasPassed()) {                     \
    if (aOther.prop.Value().EqualsLiteral("null")) { \
      prop = defaultValue;                           \
    } else {                                         \
      prop = aOther.prop.Value();                    \
    }                                                \
  } else {                                           \
    prop = defaultValue;                             \
  }

#define COPY_OPT_FIELD(prop, defaultValue) \
  if (aOther.prop.WasPassed()) {           \
    prop = aOther.prop.Value();            \
  } else {                                 \
    prop = defaultValue;                   \
  }

#define COPY_FIELD(prop) prop = aOther.prop;

    COPY_FIELD(mId)
    COPY_FIELD(mCmd)
    COPY_OPT_STRING_FIELD(mDomain, EmptyString())
    COPY_OPT_STRING_FIELD(mGateway, EmptyString())
    COPY_SEQUENCE_FIELD(mGateways, nsString)
    COPY_OPT_STRING_FIELD(mIfname, EmptyString())
    COPY_OPT_FIELD(mNetworkType, -1)
    COPY_OPT_STRING_FIELD(mIp, EmptyString())
    COPY_OPT_FIELD(mPrefixLength, 0)
    COPY_OPT_STRING_FIELD(mMode, EmptyString())
    COPY_OPT_FIELD(mReport, false)
    COPY_OPT_FIELD(mEnabled, false)
    COPY_OPT_STRING_FIELD(mWifictrlinterfacename, EmptyString())
    COPY_OPT_STRING_FIELD(mInternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mExternalIfname, EmptyString())
    COPY_OPT_FIELD(mEnable, false)
    COPY_OPT_STRING_FIELD(mSsid, EmptyString())
    COPY_OPT_STRING_FIELD(mSecurity, EmptyString())
    COPY_OPT_STRING_FIELD(mKey, EmptyString())
    COPY_OPT_STRING_FIELD(mPrefix, EmptyString())
    COPY_OPT_STRING_FIELD(mLink, EmptyString())
    COPY_SEQUENCE_FIELD(mInterfaceList, nsString)
    COPY_OPT_STRING_FIELD(mWifiStartIp, EmptyString())
    COPY_OPT_STRING_FIELD(mWifiEndIp, EmptyString())
    COPY_OPT_STRING_FIELD(mUsbStartIp, EmptyString())
    COPY_OPT_STRING_FIELD(mUsbEndIp, EmptyString())
    COPY_OPT_STRING_FIELD(mDns1, EmptyString())
    COPY_OPT_STRING_FIELD(mDns2, EmptyString())
    COPY_SEQUENCE_FIELD(mDnses, nsString)
    COPY_OPT_STRING_FIELD(mStartIp, EmptyString())
    COPY_OPT_STRING_FIELD(mEndIp, EmptyString())
    COPY_OPT_STRING_FIELD(mServerIp, EmptyString())
    COPY_OPT_STRING_FIELD(mMaskLength, EmptyString())
    COPY_OPT_STRING_FIELD(mType, EmptyString())
    COPY_OPT_STRING_FIELD(mTcpBufferSizes, EmptyString())
    COPY_OPT_STRING_FIELD(mPreInternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mPreExternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mCurInternalIfname, EmptyString())
    COPY_OPT_STRING_FIELD(mCurExternalIfname, EmptyString())
    COPY_OPT_FIELD(mThreshold, -1)
    COPY_OPT_FIELD(mIpaddr, 0)
    COPY_OPT_FIELD(mMask, 0)
    COPY_OPT_FIELD(mGateway_long, 0)
    COPY_OPT_FIELD(mDns1_long, 0)
    COPY_OPT_FIELD(mDns2_long, 0)
    COPY_OPT_FIELD(mMtu, 0)
    COPY_OPT_FIELD(mPrivacyExtensions, false)
    COPY_SEQUENCE_FIELD(mIPv6Routes, nsString)
    COPY_OPT_STRING_FIELD(mIpv6Ip, EmptyString())
    COPY_OPT_STRING_FIELD(mIPv6Prefix, EmptyString())
    COPY_OPT_STRING_FIELD(mNat64Prefix, EmptyString())
    COPY_OPT_FIELD(mNetId, -1)

    mLoopIndex = 0;

#undef COPY_SEQUENCE_FIELD
#undef COPY_OPT_STRING_FIELD
#undef COPY_OPT_FIELD
#undef COPY_FIELD
  }

  // Followings attributes are 1-to-1 mapping to NetworkCommandOptions.
  int32_t mId;
  nsString mCmd;
  nsString mDomain;
  nsString mGateway;
  CopyableTArray<nsString> mGateways;
  nsString mIfname;
  long mNetworkType;
  nsString mIp;
  uint32_t mPrefixLength;
  nsString mMode;
  bool mReport;
  bool mEnabled;
  nsString mWifictrlinterfacename;
  nsString mInternalIfname;
  nsString mExternalIfname;
  bool mEnable;
  bool mPrivacyExtensions;
  nsString mSsid;
  nsString mSecurity;
  nsString mKey;
  nsString mPrefix;
  nsString mLink;
  CopyableTArray<nsString> mInterfaceList;
  nsString mWifiStartIp;
  nsString mWifiEndIp;
  nsString mUsbStartIp;
  nsString mUsbEndIp;
  nsString mDns1;
  nsString mDns2;
  CopyableTArray<nsString> mDnses;
  nsString mStartIp;
  nsString mEndIp;
  nsString mServerIp;
  nsString mMaskLength;
  nsString mType;
  nsString mTcpBufferSizes;
  nsString mPreInternalIfname;
  nsString mPreExternalIfname;
  nsString mCurInternalIfname;
  nsString mCurExternalIfname;
  long long mThreshold;
  long mIpaddr;
  long mMask;
  long mGateway_long;
  long mDns1_long;
  long mDns2_long;
  long mMtu;
  CopyableTArray<nsString> mIPv6Routes;
  nsString mIpv6Ip;
  nsString mIPv6Prefix;
  nsString mNat64Prefix;

  // Auxiliary information required to carry accros command chain.
  int mNetId;           // A locally defined id per interface.
  uint32_t mLoopIndex;  // Loop index for adding/removing multiple gateways.
};

// CommandChain store the necessary information to execute command one by one.
// Including :
// 1. Command parameters.
// 2. Command list.
// 3. Error callback function.
// 4. Index of current execution command.
class CommandChain final {
 public:
  CommandChain(const NetworkParams& aParams, const CommandFunc aCmds[],
               uint32_t aLength, ErrorCallback aError)
      : mIndex(-1),
        mParams(aParams),
        mCommands(aCmds),
        mLength(aLength),
        mError(aError) {}

  NetworkParams& getParams() { return mParams; };

  CommandFunc getNextCommand() {
    mIndex++;
    return mIndex < mLength ? mCommands[mIndex] : nullptr;
  };

  ErrorCallback getErrorCallback() const { return mError; };

 private:
  uint32_t mIndex;
  NetworkParams mParams;
  const CommandFunc* mCommands;
  uint32_t mLength;
  ErrorCallback mError;
};

// A helper class to easily construct a resolved
// or a pending result for command execution.
class CommandResult {
 public:
  struct Pending {};

 public:
  explicit CommandResult(int32_t aResultCode);
  explicit CommandResult(const mozilla::dom::NetworkResultOptions& aResult);
  explicit CommandResult(const Pending&);
  bool isPending() const;

  mozilla::dom::NetworkResultOptions mResult;

 private:
  bool mIsPending;
};

class NetworkUtils final {
 public:
  explicit NetworkUtils(MessageCallback aCallback);
  ~NetworkUtils();

  void DispatchCommand(const NetworkParams& aParams);
  void ExecuteCommand(NetworkParams aOptions);
  void updateDebug();
  static void NotifyNetdEvent(mozilla::dom::NetworkResultOptions& aResult);

  MessageCallback getMessageCallback() { return mMessageCallback; }

 private:
  /**
   * Commands supported by NetworkUtils.
   */
  CommandResult createNetwork(NetworkParams& aOptions);
  CommandResult destroyNetwork(NetworkParams& aOptions);
  CommandResult getNetId(NetworkParams& aOptions);
  CommandResult setDefaultRoute(NetworkParams& aOptions);
  CommandResult removeDefaultRoute(NetworkParams& aOptions);
  CommandResult setDNS(NetworkParams& aOptions);
  CommandResult addHostRoute(NetworkParams& aOptions);
  CommandResult removeHostRoute(NetworkParams& aOptions);
  CommandResult removeNetworkRoute(NetworkParams& aOptions);
  CommandResult addSecondaryRoute(NetworkParams& aOptions);
  CommandResult removeSecondaryRoute(NetworkParams& aOptions);
  CommandResult setMtu(NetworkParams& aOptions);
  CommandResult setDefaultNetwork(NetworkParams& aOptions);
  CommandResult addInterfaceToNetwork(NetworkParams& aOptions);
  CommandResult removeInterfaceToNetwork(NetworkParams& aOptions);
  CommandResult setIpv6Status(NetworkParams& aOptions);
  CommandResult setIpv6PrivacyExtensions(NetworkParams& aOptions);
  CommandResult dhcpRequest(NetworkParams& aOptions);
  CommandResult stopDhcp(NetworkParams& aOptions);
  CommandResult getInterfaces(NetworkParams& aOptions);
  CommandResult getInterfaceConfig(NetworkParams& aOptions);
  CommandResult setInterfaceConfig(NetworkParams& aOptions);
  CommandResult startClatd(NetworkParams& aOptions);
  CommandResult stopClatd(NetworkParams& aOptions);
  CommandResult setTcpBufferSize(NetworkParams& aOptions);
  CommandResult setNetworkInterfaceAlarm(NetworkParams& aOptions);
  CommandResult enableNetworkInterfaceAlarm(NetworkParams& aOptions);
  CommandResult disableNetworkInterfaceAlarm(NetworkParams& aOptions);
  CommandResult setTetheringAlarm(NetworkParams& aOptions);
  CommandResult removeTetheringAlarm(NetworkParams& aOptions);
  CommandResult setDhcpServer(NetworkParams& aOptions);
  CommandResult getTetheringStatus(NetworkParams& aOptions);
  CommandResult getTetherStats(NetworkParams& aOptions);
  CommandResult setUSBTethering(NetworkParams& aOptions);
  CommandResult setWifiTethering(NetworkParams& aOptions);
  CommandResult updateUpStream(NetworkParams& aOptions);
  CommandResult removeUpStream(NetworkParams& aOptions);
  CommandResult setupPrefix64Discovery(NetworkParams& aOptions);

  /**
   * function pointer array holds all netd commands should be executed
   * in sequence to accomplish a given command by other module.
   */
  // static const CommandFunc sWifiEnableChain[]; ...
  static const CommandFunc sUSBEnableChain[];
  static const CommandFunc sUSBDisableChain[];
  static const CommandFunc sUSBFailChain[];
  static const CommandFunc sWifiEnableChain[];
  static const CommandFunc sWifiDisableChain[];
  static const CommandFunc sWifiFailChain[];
  static const CommandFunc sUpdateUpStreamChain[];
  /**
   * Individual netd command stored in command chain.
   */
#define PARAMS                                     \
  CommandChain *aChain, CommandCallback aCallback, \
      mozilla::dom::NetworkResultOptions &aResult
  // static void enableNat(PARAMS);...
  static void clearAddrForInterface(PARAMS);
  static void createNetwork(PARAMS);
  static void destroyNetwork(PARAMS);
  static void setIpv6Enabled(PARAMS);
  static void enableIpv6(PARAMS);
  static void disableIpv6(PARAMS);
  static void wakeupAddInterface(PARAMS);
  static void wakeupDelInterface(PARAMS);
  static void addInterfaceToNetwork(PARAMS);
  static void removeInterfaceToNetwork(PARAMS);
  static void addDefaultRouteToNetwork(PARAMS);
  static void removeDefaultRoute(PARAMS);
  static void setInterfaceDns(PARAMS);
  static void addRouteToInterface(PARAMS);
  static void removeRouteFromInterface(PARAMS);
  static void addRouteToSecondaryTable(PARAMS);
  static void removeRouteFromSecondaryTable(PARAMS);
  static void setMtu(PARAMS);
  static void setDefaultNetwork(PARAMS);
  static void setIpv6PrivacyExtensions(PARAMS);
  static void defaultAsyncSuccessHandler(PARAMS);
  static void setConfig(PARAMS);
  static void enableNat(PARAMS);
  static void disableNat(PARAMS);
  static void setIpForwardingEnabled(PARAMS);
  static void setIpForwardingDisabled(PARAMS);
  static void tetherInterface(PARAMS);
  static void untetherInterface(PARAMS);
  static void addRouteToLocalNetwork(PARAMS);
  static void addIpv6RouteToLocalNetwork(PARAMS);
  static void removeIpv6LocalNetworkRoute(PARAMS);
  static void removeRouteFromLocalNetwork(PARAMS);
  static void startTethering(PARAMS);
  static void stopTethering(PARAMS);
  static void setDnsForwarders(PARAMS);
  static void cleanUpStream(PARAMS);
  static void createUpStream(PARAMS);
  static void cleanUpStreamInterfaceForwarding(PARAMS);
  static void createUpStreamInterfaceForwarding(PARAMS);
  static void addIpv6TetheringInterfaces(PARAMS);
  static void removeIpv6TetheringInterfaces(PARAMS);
  static void updateIpv6Tethering(PARAMS);
  static void stopIpv6Tethering(PARAMS);
  static void usbTetheringSuccess(PARAMS);
  static void wifiTetheringSuccess(PARAMS);
  static void updateUpStreamSuccess(PARAMS);
  static void setupPrefix64Discovery(PARAMS);
#undef PARAMS

  /**
   * Error callback function executed when a command is fail.
   */
#define PARAMS \
  NetworkParams &aOptions, mozilla::dom::NetworkResultOptions &aResult
  // TODO: Error callback function.
  static void defaultAsyncFailureHandler(PARAMS);
  static void usbTetheringFail(PARAMS);
  static void wifiTetheringFail(PARAMS);
  static void updateUpStreamFail(PARAMS);
#undef PARAMS

  /**
   * Command chain processing functions.
   */
  static void next(CommandChain* aChain, bool aError,
                   mozilla::dom::NetworkResultOptions& aResult);

  /**
   * Utility functions.
   */
  void dumpParams(NetworkParams& aOptions, const char* aType);
  static bool composeIpv6TetherConf(const char* aInternalIface,
                                    const char* aNetworkPrefix,
                                    uint32_t aDnsLength);
  static bool getIpv6Prefix(const char* aIpv6Addr, char* aIpv6Prefix);
  void Shutdown();
  static void runNextQueuedCommandChain();
  static void finalizeSuccess(CommandChain* aChain,
                              mozilla::dom::NetworkResultOptions& aResult);
  template <size_t N>
  static void runChain(const NetworkParams& aParams,
                       const CommandFunc (&aCmds)[N], ErrorCallback aError);
  static nsCString getSubnetIp(const nsCString& aIp, int aPrefixLength);
  void setupNetd(bool aEnable);

  /**
   * Callback function to send netd result to main thread.
   */
  MessageCallback mMessageCallback;

  /*
   * Utility class to access libnetutils.
   */
  mozilla::UniquePtr<NetUtils> mNetUtils;

  NetIdManager mNetIdManager;
};

#endif
