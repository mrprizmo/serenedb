# IResearch search engine

## Overview

The IResearch library is meant to be treated as a standalone index that is capable of both indexing and storing
individual values verbatim.
Indexed data is treated on a per-version/per-revision basis, i.e. existing data version/revision is never modified and
updates/removals are treated as new versions/revisions of the said data. This allows for trivial multi-threaded
read/write operations on the index.
The index exposes its data processing functionality via a multi-threaded 'writer' interface that treats each document
abstraction as a collection of fields to index and/or store.
The index exposes its data retrieval functionality via 'reader' interface that returns records from an index matching a
specified query.
The queries themselves are constructed query trees built directly using the query building blocks available in the API.
The querying infrastructure provides the capability of ordering the result set by one or more ranking/scoring
implementations.

## License

Copyright (c) 2017-2023 ArangoDB GmbH

Copyright (c) 2016-2017 EMC Corporation

This software is provided under the Apache 2.0 Software license provided in the
[LICENSE.md](LICENSE.md) file. Licensing information for third-party products used
by IResearch search engine can be found in
[THIRD_PARTY_README.md](THIRD_PARTY_README.md)
