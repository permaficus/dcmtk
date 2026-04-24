/*
 *
 *  Copyright (C) 2026, Open Connections GmbH
 *  All rights reserved.  See COPYRIGHT file for details.
 *
 *  This software and supporting documentation were developed by
 *
 *    OFFIS e.V.
 *    R&D Division Health
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *
 *  Module:  dcmiod
 *
 *  Author:  Michael Onken
 *
 *  Purpose: Tests for the static per-class default IODRules optimisation
 *           (copy-on-write sharing of IODRules across instances of the same
 *           IODComponent subclass).
 *
 */

#include "dcmtk/config/osconfig.h" /* make sure OS specific configuration is included first */
#include "dcmtk/dcmdata/dcitem.h"
#include "dcmtk/dcmiod/iodmacro.h"
#include "dcmtk/dcmiod/iodrules.h"
#include "dcmtk/ofstd/ofmem.h"
#include "dcmtk/ofstd/oftest.h"

// SOPInstanceReferenceMacro is used for these tests because it:
//   * has two type-"1" rules (ReferencedSOPClassUID, ReferencedSOPInstanceUID)
//   * does NOT override check() — the base IODComponent::check() is used, which
//     iterates over the rules and honours their requirement types.
// CodeSequenceMacro was intentionally NOT chosen: its check() override performs
// a bespoke value-presence test that ignores the rules entirely.


/** Test copy-on-write isolation between two instances of the same class.
 *
 *  Both instances start by sharing the class-level static default rules.
 *  Calling makeOptional() on one triggers copy-on-write: that instance
 *  gets a private clone with all requirement types set to "3".  The other
 *  instance must still reference the unmodified static defaults.
 */
OFTEST(dcmiod_static_rules_cow)
{
    SOPInstanceReferenceMacro a, b;

    // Empty data → both type-"1" attributes absent → check() must fail.
    OFCHECK(a.check(OFTrue /* quiet */).bad());
    OFCHECK(b.check(OFTrue /* quiet */).bad());

    // makeOptional() triggers CoW on 'a': all of a's rules become type "3".
    a.makeOptional();

    // 'a' now accepts empty data.
    OFCHECK(a.check(OFTrue).good());

    // 'b' still holds the original static rules and must still fail.
    OFCHECK(b.check(OFTrue).bad());
}


/** Test that resetRules() restores the class-level static default rules.
 *
 *  After makeOptional() a copy-on-write clone exists.  resetRules() must
 *  drop that clone and re-share the static, so that requirement-checking
 *  behaves as on a freshly constructed instance.
 */
OFTEST(dcmiod_static_rules_reset)
{
    SOPInstanceReferenceMacro a;

    // Freshly constructed: type "1" rules fail on empty data.
    OFCHECK(a.check(OFTrue).bad());

    a.makeOptional();
    // After makeOptional() all rules are "3": empty data is acceptable.
    OFCHECK(a.check(OFTrue).good());

    a.resetRules();
    // After resetRules() the instance re-shares the static defaults.
    OFCHECK(a.check(OFTrue).bad());
}


/** Test that a third instance created after makeOptional() on another also
 *  starts with the correct static defaults (static was not permanently altered).
 */
OFTEST(dcmiod_static_rules_new_instance_after_cow)
{
    SOPInstanceReferenceMacro a;
    a.makeOptional();
    OFCHECK(a.check(OFTrue).good());

    // A brand-new instance must still pick up the unmodified static defaults.
    SOPInstanceReferenceMacro b;
    OFCHECK(b.check(OFTrue).bad());
}


/** Test the external-rules constructor path.
 *
 *  When an IODComponent is constructed with a caller-supplied
 *  OFshared_ptr<IODRules>, resetRules() must populate that shared container
 *  in-place (m_ExternalRules == OFTrue path) rather than replacing m_Rules
 *  with the class-level static.
 */
OFTEST(dcmiod_static_rules_external_container)
{
    OFshared_ptr<DcmItem>  item(new DcmItem());
    OFshared_ptr<IODRules> rules(new IODRules());

    // The external container starts empty.
    OFCHECK(rules->getByModule("SOPInstanceReferenceMacro").empty());

    // Constructing with the external container triggers resetRules() which
    // must copy all SOPInstanceReferenceMacro rules into 'rules'.
    SOPInstanceReferenceMacro m(item, rules);
    OFCHECK(!rules->getByModule("SOPInstanceReferenceMacro").empty());
    // SOPInstanceReferenceMacro defines exactly 2 rules.
    OFCHECK(rules->getByModule("SOPInstanceReferenceMacro").size() == 2);

    // A second instance sharing the same container must not duplicate rules
    // (addRule is called with overwriteExisting = OFTrue).
    SOPInstanceReferenceMacro m2(item, rules);
    OFCHECK(rules->getByModule("SOPInstanceReferenceMacro").size() == 2);
}
