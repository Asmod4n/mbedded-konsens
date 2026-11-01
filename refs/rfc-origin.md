# Origin of the RFCs

Each file rfcNNNN.txt here is the text of that RFC, unchanged. The
IETF Trust allows a copy of an RFC in full (Trust Legal Provisions;
from memory, the section is not checked here). The files come from the Ubuntu packages of the RFCs:

| Package | Version | SHA-256 of the .deb |
|---|---|---|
| doc-rfc-std-proposed | 20230121-1 | 4842604e2cdab63a6eeb7e33015a1051d51a5b113c3bcd6e500a8ec35b544c4c |
| doc-rfc-std | 20230121-1 | 3901820b9e8cdeb229a25b7444a51c327b48688449570a049d6bca0e813f76d9 |
| doc-rfc-old-std | 20230121-1 | dabbd573c4f7b71ca9fc793286c54e2a8067dfac62951a69a491493d40f8d4a4 |

The digests agree with the Packages index of Ubuntu noble multiverse,
and that index agrees with the InRelease file, whose signature gpgv
checked against the Ubuntu archive keyring (2026-10-29).

## RFC 9457

RFC 9457 was published after the packages. refs/rfc9457.txt comes
from https://www.rfc-editor.org/rfc/rfc9457.txt, downloaded on
2026-10-29. Its SHA-256 is
f2b3db92fb0bf3489cb3841a0da0c0d88dff40797b64d40b6123085183886c7b.

## Errata

The errata come from https://www.rfc-editor.org/errata.json, which
redirects to https://www.rfc-editor.org/api/v1/errata.json. It was
downloaded on 2026-10-29. Its SHA-256 is
d555caa789fb1e20d088f57a71de88842ac86da7d3b182bdf4e24dc7cacc23c5.
The source gives no file for one RFC. Each file rfcNNNN-errata.json
here is a JSON array of the records of that RFC, in the order of the
source. Each record is the bytes of the source, unchanged.

The errata IDs in each file agree with the page
https://www.rfc-editor.org/errata/rfcNNNN, read on 2026-10-29.

| RFC | Errata | File |
|---|---|---|
| 862 | none | - |
| 3720 | 241, 4332, 7622 | rfc3720-errata.json |
| 6455 | 18 records | rfc6455-errata.json |
| 6761 | 4970, 5025, 5039 | rfc6761-errata.json |
| 7143 | none | - |
| 7252 | 9 records | rfc7252-errata.json |
| 7641 | none | - |
| 8323 | none | - |
| 8742 | none | - |
| 8746 | none | - |
| 8949 | 8589 | rfc8949-errata.json |
| 9112 | 7214, 7633, 7744, 8284 | rfc9112-errata.json |
| 9290 | none | - |
| 9457 | 7731, 8858 | rfc9457-errata.json |

"none" means that errata.json has no record for that RFC, and that its
page says "No matching errata found." (2026-10-29).

## RFC 9110

refs/rfc9110.txt comes from doc-rfc-std 20230121-1 (the digest in the
table above, checked again on 2026-10-30), file
usr/share/doc/RFC/standard/rfc9110.txt.gz. Its errata are not here:
www.rfc-editor.org could not be reached from the session on
2026-10-30.
