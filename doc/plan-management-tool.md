# Plan: the management tool in three levels

Status: draft. Nothing is built. The owner answers the open questions
in part 6 first. A second agent reviews the plan after the answers and
before the build.

## 1. Facts checked in this session

Decisions of the owner that hold:

- The management tool is one binary of its own. It only manages. The
  node binary has no user interface (owner, 2026-10-24 12:13, 12:14).
- The user interface is in a webview where a webview can be used. A
  TUI and a CLI are the two other levels (owner, 2026-10-26 14:14).
- The tool runs on a phone with no installation (owner, 2026-10-24
  20:59). Push goes through Web Push with VAPID, with no paid account
  (owner, 20:57 to 21:00). Access goes through the VPS by VPN (owner,
  21:04).
- The tool writes the configs of all nodes, with one seed list and
  distinct node IDs, when it gets no cluster config (owner, 2026-10-24
  12:11).
- The tool finds a node in this order: the node that the operator set
  in the stored config of the tool; a node seen by the discovery
  beacon (7150/udp); the nearest node by subnet and hops; the seed
  node. A hello to all nodes, where the first answer wins, is the
  alternative (owner, 2026-10-25 18:56 to 19:05; task 61be2c4f in
  Claude-state).
- The tool shows a full disk ("no storage"), the single-node stages
  with decided_idx, accepted_idx and n_accepted, and each hardware
  finding with its evidence (owner, 2026-10-17).
- After an automatic repair, the tool shows a message. The operator
  allows the next automatic repair, or no repair happens (owner,
  2026-10-24 20:57).
- The single-node stages start only by a flag at the start of the node
  binary (owner, 2026-10-17 21:09). The tool shows them. The tool does
  not start them.
- The smallest server is 1 vCPU, 1 GB RAM and 40 GB SSD (owner,
  2026-10-25 19:08).
- The first tests use plain TCP. TLS comes later (owner, 2026-10-17
  20:52).

Packages, read in this session:

| part | library | Ubuntu 26.04 | Debian 13 |
|---|---|---|---|
| CLI options | CLI11 | yes (Launchpad) | not checked |
| TUI | FTXUI | yes (Launchpad) | yes, libftxui-dev |
| TUI | FINAL CUT | yes (Launchpad, noble and resolute) | yes, libfinal-dev |
| TUI | notcurses | 24.04 has 3.0.7 | not checked |
| webview, Linux | WebKitGTK 6.0 | 24.04 has 2.52.6 | not checked |
| webview, all OS | webview/webview | no source package | no package |

webview/webview has no package.

The webview comes from ascaridol (owner, 2026-10-26 14:20: "the
webview works everywhere, look at ascaridol"). Read in this session,
ascaridol at 8dd27ee:

- An mruby gem. It carries webview/webview as the submodule
  vendor/webview: WebView2 on Windows, WKWebView on macOS, WebKitGTK
  on Linux.
- `Ascaridol.bind` and `bind_async` give Ruby functions to the page.
- The `rb-*` router sends `rb-get`, `rb-post`, `rb-put`, `rb-patch`
  and `rb-delete` from the page to one Ruby block as (method, path,
  params). The block returns HTML.
- `Ascaridol.poll_add` watches a socket in the run loop of the window.
- It depends on mruby-cbor, mruby-lmdb, mruby-mustache and
  mruby-socket.

## 2. Form

One core and three front ends. The core is a C++ library inside the
tool. It does all work. Each front end only shows and asks.

```
          +-- cli   (one command, one answer, then exit)
core -----+-- tui   (one terminal, live view)
          +-- web   (an HTTP server; a page for the phone and
                     for the webview)
```

The core:

1. Finds a node, in the order of part 1.
2. Speaks the management protocol to that node (question 6.3). Every
   node answers the management protocol (owner, 2026-10-26 14:41).
3. Reads the state of the cluster: nodes, leader, ballot, decided_idx,
   accepted_idx, n_accepted, disk use, findings, repairs.
4. Sends the commands of part 3.
5. Writes the configs of a new cluster.

The route handlers keep no state between requests. Each request reads
what it shows from a node (owner, 2026-10-26 15:42: with no state in
the server or the client, any server of a pool can answer).
The only state the tool needs is the permission to connect and to run
management commands (owner, 15:43). Before TLS, the VPN is that
permission (question 6.7). Where it is kept later is open.

So the tool is an mruby binary. The core is C++ and has an mruby
binding. The front ends are Ruby.

The page is written once, with `rb-*` attributes and mustache
templates. One set of Ruby route handlers answers (method, path,
params) and returns HTML:

- On the desktop, ascaridol shows the page in the webview. The router
  calls the handlers through the bridge. No port is open.
- On the phone, mruby-webmachine serves the page. The router sends the
  same requests as HTTP, and a webmachine resource calls the same
  handlers (question 6.8).

One source gives two builds. The desktop build links ascaridol. The
build for the VPS has no webview, so it needs no GTK and no WebKit on
a server with 1 GB RAM. It has the CLI, the TUI and the web level.
This split is provisional.

The tool runs where the operator is, or on the VPS for the phone. On
the VPS it runs as a service with the web level on. The phone opens
the page through the VPN.

The phone constrains nothing in this plan (owner, 2026-10-26 14:43).
The phone parts - the web level over HTTP, the build for the VPS and
Web Push - come last. They may turn out to follow from the rest at no
cost, or to need another form than this plan names.

## 3. What the first version does

Read only:

- `status`: the nodes, the leader, the indexes and the stage of each
  node.
- `disk`: the use of the disk on each node and "no storage".
- `findings`: the hardware findings with their evidence.
- `watch`: the same as `status`, as a stream (TUI and web only, or
  CLI with one line per change).

Changes:

- `allow-repair`: the operator allows the next automatic repair on
  one node.
- `init`: writes the configs of a new cluster (seed list, node IDs,
  leader priority).
- `connect`: sets the target node in the stored config of the tool.

Not in the first version: a reconfiguration, a stage change, an
action on application data, Web Push. Each comes as a task of its own.

The names of the commands are provisional (question 6.6).

## 4. Order of the build

1. Core and CLI, with tests against the three-node example. The CLI is
   the level that a test can call.
2. The mruby binding of the core, and the route handlers.
3. The webview through ascaridol.
4. The web level through mruby-webmachine.
5. TUI on the same core.
6. Web Push.

Each step is its own branch and is green before the next starts.

## 5. Not in this plan

- The HTTPS part for users of an application. It is open.
- TLS between the tool and a node.
- The VPN setup on the VPS.

## 6. Open questions

6.1 Where the page for the phone comes from. Open until the owner
  needs the tool on the phone (owner, 2026-10-26 14:42). One form the
  owner names: an actor that starts mruby-webmachine and connects to
  the management port inside the cluster (owner, 14:46).
  a) The tool runs as a service on the VPS and serves it. Proposal.
  b) Each node serves it. This breaks "the node binary has no user
     interface".

6.2 The HTTP server of the web level. Answered (owner, 2026-10-26
  14:40): mruby-webmachine.
  a) mruby-webmachine. Proposal: the tool is an mruby binary, and the
     route handlers are Ruby.
  b) A small C++ HTTP library from a package.

6.3 The protocol between the tool and a node. Answered (owner,
  2026-10-26 14:40): a).
  a) The node-to-node wire: a fixed head and packed CBOR. Proposal:
     the node then needs no HTTP.
  b) HTTP on the node.

6.4 The webview. Answered (owner, 2026-10-26 14:20): ascaridol on
  every system. webview/webview comes with it as a submodule of
  ascaridol.

6.5 The TUI library. Answered (owner, 2026-10-26 15:18): FINAL CUT
  (C++, LGPL-3.0, libfinal-dev in Debian 12 and 13 and in Ubuntu 24.04
  and 26.04). The owner wants the form of Turbo Vision: a menu bar,
  Alt keys, a status line of function keys, windows and dialogs. A
  first screen shows whether FINAL CUT looks good enough. tvision has
  no clear licence for the code of Borland; FTXUI has no menu bar.
  An earlier answer (14:40) was FTXUI.
  a) FTXUI. Proposal: C++, a package in Ubuntu and Debian.
  b) notcurses.
  c) ncurses.

6.6 The name of the binary and of the commands. Answered (owner,
  2026-10-26 14:40): `konsensctl`. No specification gives
  them. Proposal: `konsensctl`, after the `ctl` form of `systemctl`,
  and the commands of part 3.

6.7 Who may manage, before TLS exists. Answered (owner, 2026-10-26
  14:40): a).
  a) Everyone who reaches the port. The VPN is the only guard.
     Proposal for the first tests only.
  b) A shared key from the start (libhydrogen).

6.8 The page on the phone and in the webview. Answered (owner,
  2026-10-26 14:32): a), and the `rb-*` router goes away.
  The owner asks for htmx 4 (2026-10-26 14:30). htmx 2 sends with XHR,
  and a webview has no server to answer it. So ascaridol has its own
  `rb-*` router.
  htmx 4.0.0 (npm, 2026-08-28, BSD-0-Clause) sends with `ctx.fetch`.
  It sets `ctx.fetch ||= window.fetch` and fires
  `htmx:before:request` before the call (dist/htmx.js:576-581). An
  extension can replace `ctx.fetch` in that event. The official
  extension hx-preload does that (dist/ext/hx-preload.js:85). The
  field is not in htmx.d.ts.
  Tested in one file outside the tree: a page from file://, htmx 4 and
  an extension of 15 lines. The extension calls a Ruby binding as
  (method, path, params) and returns `new Response(html)`. In headless
  Chromium, an hx-post of a form and an hx-get were swapped, with no
  network request and no error. WebKit (WebKitGTK, WKWebView) is not
  tested: this machine has no WebKit browser.
  a) htmx 4 with that extension in the webview, and plain htmx 4 over
     HTTP on the phone. The page has `hx-*` attributes only, and the
     `rb-*` router is not needed. The extension goes into ascaridol.
     Proposal.
  b) The `rb-*` router of ascaridol gets a `fetch` transport.

## 7. The change in ascaridol

Second test, 2026-10-26, WebKitGTK 2.52.6 with GTK 4.14.5 (Ubuntu
24.04 packages), webview/webview at cbbdee4, under Xvfb. The page is
set with `set_html`, as `Ascaridol.html=` does, so its location is
about:blank. Result:

- Without a `<base>` element, the hx-post reached the binding, and the
  hx-get failed: htmx 4 parses the path of a GET with
  `new URL(action, document.baseURI)` (dist/htmx.js:519), and
  about:blank is no base.
- With `<base href="http://ascaridol.invalid/">`, the hx-post of a form
  and the hx-get with a query were swapped. The extension took the
  path and the query from the URL. No network request left the page.
  `.invalid` is reserved by RFC 6761 6.4 and never resolves.

Not tested: WKWebView (macOS) and WebView2 (Windows). WebView2 is
Chromium, and headless Chromium passed the first test.

The change, on a branch of its own in ascaridol:

1. Add htmx 4.0.0 to the gem, verbatim, with its licence
   (BSD-0-Clause). (question 7.1)
2. Add one helper that returns the `<base>` element, htmx and the
   extension as one string for `<head>`. Provisional name:
   `Ascaridol.htmx(bind_name)`. (question 7.2)
3. The extension calls the bound Ruby block with (method, path,
   params), as the `rb-*` router does today. The block returns HTML,
   or [status, html]. Params are the query and the form fields.
4. Remove `html_router` and `ROUTER_TEMPLATE`.
5. Rewrite the four examples that use the router (dashboard, forms,
   notes, tabs_and_modals) to `hx-*`. The swaps they use exist in
   htmx: afterend, beforeend, delete, outerHTML, and the trigger blur.
6. Update the README and test/ascaridol.rb.
7. Run each example under Xvfb with WebKitGTK and compare the result
   of each interaction with the result before the change.

Questions:

7.1 How htmx comes into the page. Answered (owner, 2026-10-26 14:38):
  a).
  a) The gem carries htmx.js and embeds it. The page needs no network.
     Proposal.
  b) The application author loads htmx. The gem carries only the
     extension.

7.2 The name of the helper. Answered (owner, 2026-10-26 14:38): as
  proposed.
  `Ascaridol.htmx(bind_name)`, after the library it returns.

## 8. Review of part 7 (second agent, 2026-10-26) and changes

The review found 8 faults. Each one is changed as follows.

1. A `<base>` element sends every plain link and form to
   ascaridol.invalid, and hx-boost compares with `location.origin`
   (htmx.js:973-981). Change: no `<base>`. In `htmx:config:request`
   (htmx.js:510, before the parse at :519), the extension makes the
   action absolute against `http://ascaridol.invalid/`.
2. Over HTTP the bridge does not exist. Change: the extension reads
   `window[bind_name]` at each call and does nothing when it is not a
   function. The phone page loads plain htmx 4 and no extension.
3. A Ruby error was swapped into the target by the old router; with
   `ctx.fetch` rejecting, htmx swaps nothing (:609). Change: the
   extension returns status 500 with the escaped message in
   `<div class="error">`. htmx 4 swaps 4xx and 5xx (:202).
4. The block may return a String, [status, html] or nil. Change: all
   three are read; nil and 204, 205, 304 give a response with no body.
5. `rb-indicator` gave the class `busy`; htmx gives `htmx-request`
   (:194). The old router disabled buttons; htmx needs `hx-disable`.
   Change: the examples take both forms of htmx. `rb-get=""` in
   tabs_and_modals.rb becomes `hx-on:click="this.remove()"`.
6. A POST from an input inside a form sends the whole form (:479-481);
   GET and DELETE send the query (:476). Change: the README says so. A
   file value is refused with a TypeError.
7. `hx-push-url` cannot push a URL of another origin on about:blank
   (:1615-1619). Change: the helper sets `history: false` in the
   htmx config. The README says push-url works over HTTP only.
8. The timeout of htmx aborts `req.signal` (:196). Change: the call
   races the signal and rejects with an AbortError.

Third test, 2026-10-26, WebKitGTK 2.52.6, the changed extension, no
`<base>`: hx-post of a form, hx-get with a query, a String return, a
raised error (swapped as escaped text) and a nil return (no swap)
behave as above, and `<a href="#x">` stays on about:blank#x. The
abort path is not tested.
