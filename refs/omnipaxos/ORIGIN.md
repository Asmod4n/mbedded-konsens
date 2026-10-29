# Origin

The models follow the Omni-Paxos source at one commit. The directory
omnipaxos/src of that commit stands here verbatim, under
refs/omnipaxos/omnipaxos/src, with the license of the upstream source in
refs/omnipaxos/LICENSE. Nothing in it is changed. Each citation in the
models and the results names a file and a line at this commit.

- Upstream repository: https://github.com/haraldng/omnipaxos
- Commit: 532ff79a586a60c1f9fa1cf58df1c829e5a6882f
- Source tree at that commit: https://github.com/haraldng/omnipaxos/tree/532ff79a586a60c1f9fa1cf58df1c829e5a6882f/omnipaxos/src
- License of the upstream source: Apache-2.0

A citation such as `leader.rs:180` means
`omnipaxos/src/sequence_paxos/leader.rs`, line 180, at this commit.

## Reported upstream

- S7: at this commit, the decided_idx of a follower can fall when a late
  Promise makes the leader send AcceptSync. The fault was reported
  upstream on 2026-10-17. The maintainer fixes it in a pull request of
  his own.
