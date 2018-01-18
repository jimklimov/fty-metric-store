# fty-metric-store

Agent fty-metric-store stores metrics to DB and provides an interface to request them.

## How to build

To build fty-metric-store project run:

```bash
./autogen.sh
./configure
make
make check # to run self-test
```

## How to run

To run fty-metric-store project:

* from within the source tree, run:

```bash
./src/fty-metric-store
```

For the other options available, refer to the manual page of fty-metric-store

* from an installed base, using systemd, run:

```bash
systemctl start fty-metric-store
```

Agent also contains script set up as timer service for cleaning up old metrics: fty-metric-store-cleaner.

For further information, refer to the manual page of fty-metric-store-cleaner.

### Configuration file

Configuration file - fty-metric-store.cfg - is currently ignored.

Agent reads environment variable BIOS\_LOG\_LEVEL to set verbosity level.

## Architecture

### Overview

fty-metric-store has 1 actor:

* fty-metric-store-server: main actor

It also has one built-in timer, which checks the cache of pending metrics every second.  
If it contains too much data/enough time passed, inserts metrics into DB.

## Protocols

### Published metrics

Agent doesn't publish any metrics.

### Published alerts

Agent doesn't publish any alerts.

### Mailbox requests

Agent fty-metric-store can be requested for:

* getting metrics for specified device and topic, of specified type and step,  
from the specified time interval

#### Getting metrics

The USER peer sends the following message using MAILBOX SEND to
FTY-METRIC-STORE-SERVER ("fty-metric-store") peer:

* zuuid/GET/asset/topic/step/type/start/end/ordering\_flag  
    - request metrics for specified device and topic, of specified type and step,  
    from the specified time interval

where
* '/' indicates a multipart string message
* 'asset' is internal name of an asset
* 'topic' is what the metric measures. e.g. realpower.default
* 'step' is interval over which the metric was computed (15m, 24h, 7d, 30d)
* 'type' MUST be one of the (min, max, arithmetic\_mean)
* 'start' MUST be UTC unix timestamp
* 'end' MUST be UTC unix timestamp
* 'start' < 'end'
* 'ordering\_flag' MUST be 0 or 1
* subject of the message MUST be "aggregated data".

The FTY-METRIC-STORE-SERVER peer MUST respond with one of these messages back to USER
peer using MAILBOX SEND.

* zuuid/OK/asset/topic/step/type/start/end/ordering\_flag/unit/[timestamp-i/value-i]
* zuuid/ERROR/reason

where
* '/' indicates a multipart frame message
* 'zuuid','asset','topic','step','type','start','end','ordering\_flag' MUST be repeated from request
* 'unit' MUST be unit of requested metric
* 'timestamp' MUST be timestamp of the metric sent
* 'value' MUST be value of the metric sent
* 'reason' MUST be reason for error
* subject of the message MUST be "aggregated data".

### Stream subscriptions

# METRICS stream

If the metric did not come from fty-metric-compute, ignore it.

If it did, insert it into DB.

# ASSETS stream

If ASSET DELETE came, delete all topics and measurements for this asset.
