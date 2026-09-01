# Contributing to Native Logger Quest

Native Logger Quest is distributed under GPL-3.0-only with the preserved Big
Screen GPLv3 section 7 terms in `LICENSE-ADDITIONAL-TERMS.md`.

By intentionally submitting a contribution, you license that contribution to
Loud160 (AKA Whisp), the Native Logger Quest project, and its maintainer under
the standard MIT terms in `INBOUND_LICENSE.md`, in addition to the outbound
project license. This is a license grant, not a copyright assignment.

Every commit must also certify provenance under Developer Certificate of
Origin 1.1 by using `git commit -s`. The DCO certification and inbound MIT
grant are separate requirements.

Keep the logger independent from Paper2, Beat Saber, Unity, BSML, SongCore,
and other mod APIs. Android-specific output may use the platform `liblog` API;
the file writer and tests must remain host-buildable.
