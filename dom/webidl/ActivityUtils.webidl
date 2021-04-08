/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain at http://mozilla.org/MPL/2.0/. */

[JSImplementation="@mozilla.org/activity-utils;1",
 Exposed=Window]
interface ActivityUtils
{
  /* Get an array of apps that register as an activity handler of the given
   * activity name.
   * @param name: activity name.
   * @return a promise, resolve an array of objects described below,
   *         or reject with error.
   *   object:
   *    {
   *      "manifest": manifest of the app, eg: "http://music.localhost/manifest.webmanifest"
   *      "description": whatever specified to the activity name in manifest.webmanifest.
   *      "icon": deprecated, always an empty string.
   *    }
   */
  Promise<sequence<object>> getInstalled(DOMString name);
};
