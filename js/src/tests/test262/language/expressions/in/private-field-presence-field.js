// |reftest| shell-option(--enable-ergonomic-brand-checks) shell-option(--enable-private-fields) skip-if(!xulRuntime.shell) -- requires shell-options
// Copyright 2021 the V8 project authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Value when private name describes a field
info: |
  7. Let privateName be ? GetValue(privateNameBinding).
  8. Assert: privateName is a Private Name.
  9. If privateName.[[Kind]] is "field",
     a. If ! PrivateFieldFind(privateName, rval) is not empty, then return true.
  [...]
  11. Return false.
esid: sec-relational-operators-runtime-semantics-evaluation
features: [class-fields-private, class-fields-private-in]
---*/

class Class {
  #field;

  static isNameIn(value) {
    return #field in value;
  }
}

assert.sameValue(Class.isNameIn({}), false);
assert.sameValue(Class.isNameIn(new Class()), true);

reportCompare(0, 0);
