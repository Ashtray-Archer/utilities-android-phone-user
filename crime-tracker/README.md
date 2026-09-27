# Crime tracker

Separate branch for researching a public-safety / incident-feed utility.

Keep this separate from `civics` for now. Government agendas, legislation, and
meeting records have a different source model from incident data or live radio.

Initial research scope:

- public incident/open-data feeds published by agencies or local governments;
- Washtenaw County's public Crime Mapping portal;
- existing open-source crime-map/scanner projects worth learning from;
- only later, if useful, live scanner/radio ingestion as its own acquisition
  boundary.

No incident scraper, scanner receiver, APK, or deployment path is implemented on
this branch yet.

When this moves beyond research, preserve the agency's own event category and
time/location fields, retain source provenance, and distinguish reports,
dispatches, citations, arrests, charges, and convictions rather than flattening
them into a single claim that a person committed a crime.
