# SPDX-License-Identifier: Apache-2.0
"""Markdown and self-contained HTML reports. All untrusted strings (test-case
bytes, backtraces, paths) are escaped; the HTML has no external resources."""
from __future__ import annotations

import datetime as _dt
import html
from pathlib import Path

from .triage import Finding, summarize


def _md_escape(s: str) -> str:
    return str(s).replace("|", "\\|").replace("\n", " ")


def _repro_cmd(f: Finding, target: str, spawn: str | None) -> str:
    rf = f.replay_file
    file = Path(rf).name if rf else "CASE.rfz"
    sp = f' --spawn "{spawn}"' if spawn else ""
    sub = {"crash": "crashes", "hang": "hangs", "deadline": "deadlines"}[f.type]
    return f"resfuzz-agent --target {target}{sp} --replay {sub}/{file}"


def render_markdown(findings: list[Finding], stats: dict, meta: dict) -> str:
    s = summarize(findings)
    target, spawn = meta.get("target", "?"), meta.get("spawn")
    lines = [
        f"# resfuzz report: `{_md_escape(target)}`",
        "",
        f"Generated {meta.get('generated', '')} on platform `{_md_escape(stats.get('platform', '?'))}`.",
        "",
        "| Metric | Value |", "|---|---|",
        f"| Executions | {stats.get('execs', 0)} ({stats.get('execs_per_s', 0)}/s over {stats.get('elapsed_s', 0)} s) |",
        f"| Coverage | {'on, ' + str(stats.get('edges', 0)) + ' edges' if stats.get('coverage') else 'off (reply-signature feedback)'} |",
        f"| Reply signatures | {stats.get('reply_signatures', 0)} |",
        f"| Deadline | {stats.get('deadline_us', '?')} us |",
        f"| Max latency seen | {stats.get('max_latency_us', 0)} us |",
        f"| Target restarts | {stats.get('respawns', 0)} |",
        f"| Unique findings | {s['crash']} crash, {s['hang']} hang, {s['deadline']} deadline ({s['reproducible']} reproducible) |",
        "",
    ]
    if not findings:
        lines.append("No findings.")
    else:
        lines += ["## Findings", "", "| # | Severity | Finding | Hits | Reproducible |", "|---|---|---|---|---|"]
        for i, f in enumerate(findings, 1):
            rep = f.repro_label
            lines.append(f"| {i} | {f.severity} | {_md_escape(f.title)} | {f.count} | {rep} |")
        for i, f in enumerate(findings, 1):
            lines += ["", f"### {i}. {_md_escape(f.title)}", "",
                      f"- Bucket `{f.bucket}`, first seen at {f.first_t_ms / 1000:.1f} s, {f.count} hit(s)",
                      f"- Errno: {f.errno} {f.errno_name}".rstrip()]
            if f.signal:
                lines.append(f"- Signal: {f.signal_name}")
            if f.type == "deadline":
                lines.append(f"- Worst reply latency: {f.max_latency_us} us (deadline {stats.get('deadline_us', '?')} us)")
            lines += [f"- Reproduce: `{_repro_cmd(f, target, spawn)}`"]
            if f.seq_reproducible and not f.reproducible:
                lines.append(f"- Stateful: needs earlier messages; reproduces deterministically when the recorded "
                             f"{f.seq_cases}-case history is replayed against a fresh server")
            tc = f.minimized or f.testcase
            if tc is not None:
                label = "Minimised reproducer" if f.minimized else "Test case"
                lines += ["", f"{label} ({len(tc.ops)} ops):", "", "```", tc.describe(), "```"]
            if f.frames:
                lines += ["", "Backtrace:", "", "```"]
                lines += [f"#{fr.index} {fr.func}" + (f" at {fr.file}:{fr.line}" if fr.file else "") for fr in f.frames[:12]]
                lines.append("```")
    return "\n".join(lines) + "\n"


_CSS = """body{font-family:system-ui,sans-serif;max-width:1000px;margin:2em auto;padding:0 1em;color:#1b1b1b}
table{border-collapse:collapse;margin:1em 0}td,th{border:1px solid #ccc;padding:.3em .6em;text-align:left}
pre{background:#f5f5f5;padding:.8em;overflow:auto}.sev-high{color:#b00020;font-weight:bold}
.sev-medium{color:#b26a00}.sev-low{color:#555}code{background:#f0f0f0;padding:0 .2em}"""


def render_html(findings: list[Finding], stats: dict, meta: dict) -> str:
    e = html.escape
    s = summarize(findings)
    target, spawn = meta.get("target", "?"), meta.get("spawn")
    rows = "".join(
        f"<tr><td>{i}</td><td class='sev-{e(f.severity)}'>{e(f.severity)}</td><td><a href='#f{i}'>{e(f.title)}</a></td>"
        f"<td>{f.count}</td><td>{e(f.repro_label)}</td></tr>"
        for i, f in enumerate(findings, 1))
    details = []
    for i, f in enumerate(findings, 1):
        tc = f.minimized or f.testcase
        parts = [f"<h3 id='f{i}'>{i}. {e(f.title)}</h3><ul>",
                 f"<li>Bucket <code>{e(f.bucket)}</code>, {f.count} hit(s), first at {f.first_t_ms / 1000:.1f} s</li>",
                 f"<li>Errno {f.errno} {e(f.errno_name)}</li>"]
        if f.signal:
            parts.append(f"<li>Signal {e(f.signal_name)}</li>")
        if f.type == "deadline":
            parts.append(f"<li>Worst reply latency {f.max_latency_us} us</li>")
        parts.append(f"<li>Reproduce: <code>{e(_repro_cmd(f, target, spawn))}</code></li></ul>")
        if tc is not None:
            parts.append(f"<p>{'Minimised reproducer' if f.minimized else 'Test case'} ({len(tc.ops)} ops)</p>"
                         f"<pre>{e(tc.describe())}</pre>")
        if f.frames:
            bt = "\n".join(f"#{fr.index} {fr.func}" + (f" at {fr.file}:{fr.line}" if fr.file else "") for fr in f.frames[:12])
            parts.append(f"<p>Backtrace</p><pre>{e(bt)}</pre>")
        details.append("".join(parts))
    cov = f"on, {stats.get('edges', 0)} edges" if stats.get("coverage") else "off (reply-signature feedback)"
    return f"""<!doctype html><html lang="en"><head><meta charset="utf-8">
<title>resfuzz report: {e(target)}</title><style>{_CSS}</style></head><body>
<h1>resfuzz report: <code>{e(target)}</code></h1>
<p>Generated {e(meta.get('generated', ''))} on platform <code>{e(str(stats.get('platform', '?')))}</code>.</p>
<table><tr><th>Executions</th><td>{stats.get('execs', 0)} ({stats.get('execs_per_s', 0)}/s over {stats.get('elapsed_s', 0)} s)</td></tr>
<tr><th>Coverage</th><td>{e(cov)}</td></tr>
<tr><th>Deadline</th><td>{e(str(stats.get('deadline_us', '?')))} us, worst latency seen {stats.get('max_latency_us', 0)} us</td></tr>
<tr><th>Target restarts</th><td>{stats.get('respawns', 0)}</td></tr>
<tr><th>Unique findings</th><td>{s['crash']} crash, {s['hang']} hang, {s['deadline']} deadline ({s['reproducible']} reproducible)</td></tr></table>
<h2>Findings</h2>
{"<table><tr><th>#</th><th>Severity</th><th>Finding</th><th>Hits</th><th>Reproducible</th></tr>" + rows + "</table>" if findings else "<p>No findings.</p>"}
{''.join(details)}
</body></html>
"""


def write_reports(out_dir: str | Path, findings: list[Finding], stats: dict, meta: dict) -> tuple[Path, Path]:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    meta = dict(meta)
    meta.setdefault("generated", _dt.datetime.now().astimezone().strftime("%Y-%m-%d %H:%M %Z"))
    md, ht = out / "report.md", out / "report.html"
    md.write_text(render_markdown(findings, stats, meta))
    ht.write_text(render_html(findings, stats, meta))
    return md, ht
