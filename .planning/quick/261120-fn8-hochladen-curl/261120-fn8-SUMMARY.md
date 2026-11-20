# Quick 261120-fn8 Summary

`ablegen()` in `plattform_abgleich.py` hat jetzt einen curl-Ausweg bei CERTIFICATE_VERIFY_FAILED (PUT, Temp-Datei im finally gelöscht, Argumentliste ohne Shell, Erfolg nur bei curl-Rückgabe 0 und HTTP 200/201, Fehlermeldung ohne Query/Signatur). Gemeinsamer Helfer `_ist_zertifikatsfehler` wird auch von `herunterladen` genutzt.

- Commit: 46deaf6 (nicht gepusht)
- Tests: 3 neue (curl-Aufruf, 403 ohne `sig=`, anderer URLError ohne curl); Gesamtlauf 190 passed, 2 skipped.
- Abweichungen: keine.
