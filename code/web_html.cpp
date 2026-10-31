#include "config_types.h"

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
  <title>SMS Forwarding</title>
  <style>
    :root {
      --font-ui: -apple-system, BlinkMacSystemFont, "SF Pro Text", "PingFang SC", "Hiragino Sans GB", "Segoe UI", sans-serif;
      --font-mono: "SF Mono", "SFMono-Regular", Menlo, Consolas, monospace;
      --ink: #142235;
      --body: #3d5064;
      --mute: #52667a;
      --canvas: #f8fbfe;
      --canvas-soft: rgba(255,255,255,.44);
      --canvas-soft-2: rgba(244,250,255,.68);
      --hairline: rgba(35,70,103,.12);
      --hairline-strong: rgba(35,70,103,.24);
      --link: #0a84ff;
      --success: #167b52;
      --warning: #a66200;
      --error: #c9312a;
      --ambient:
        radial-gradient(1100px 740px at 82% -12%, rgba(64,156,255,.22), transparent 62%),
        radial-gradient(900px 680px at 104% 42%, rgba(52,193,180,.16), transparent 60%),
        radial-gradient(860px 720px at 4% 106%, rgba(255,179,122,.20), transparent 64%),
        linear-gradient(145deg, #f9fcff 0%, #eef5fb 52%, #f8f3ed 100%);
      --glass-light: linear-gradient(145deg, rgba(255,255,255,.78), rgba(244,250,255,.48) 46%, rgba(255,247,240,.40));
      --glass-dark: linear-gradient(145deg, rgba(16,29,46,.86), rgba(22,41,61,.68));
      --glass-edge: rgba(255,255,255,.64);
      --glass-edge-dark: rgba(255,255,255,.16);
      --glass-blur: 26px;
      --glass-sat: 168%;
      --glass-shadow: inset 0 1px 0 rgba(255,255,255,.80), 0 24px 64px -32px rgba(26,63,95,.42), 0 2px 10px rgba(26,63,95,.07);
      --glass-shadow-dark: inset 0 1px 0 rgba(255,255,255,.14), 0 28px 72px -34px rgba(3,14,28,.72);
      --nav-w: 248px;
      --shell-inset: 16px;
      --content-max: 960px;
      --radius-sm: 12px;
      --radius-md: 18px;
      --radius-lg: 26px;
      --radius-xl: 30px;
      --radius-pill: 999px;
      --space-1: 4px;
      --space-2: 8px;
      --space-3: 12px;
      --space-4: 16px;
      --space-5: 20px;
      --space-6: 24px;
      --space-8: 32px;
      --control-h: 44px;
      --ease-out: cubic-bezier(.22,1,.36,1);
      --dur-press: 90ms;
      --dur-ui: 180ms;
      --dur-panel: 220ms;
      --focus-ring: 0 0 0 3px rgba(10,132,255,.22);
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    html { color-scheme: light; }
    .icon-sprite { position: absolute; width: 0; height: 0; overflow: hidden; }
    .ui-icon {
      width: 1.15em; height: 1.15em; display: inline-block; flex: none;
      color: currentColor; fill: none; stroke: currentColor; stroke-width: 1.8;
      stroke-linecap: round; stroke-linejoin: round; vertical-align: -.16em;
    }
    .sidebar-nav a .ico .ui-icon { width: 17px; height: 17px; color: rgba(255,255,255,.78); }
    .sidebar-nav a.active .ico .ui-icon { color: #fff; }
    .card-header .ui-icon { width: 18px; height: 18px; color: #4d5d6d; }
    .sidebar-footer .ui-icon { width: 17px; height: 17px; }
    body {
      min-height: 100vh; min-height: 100dvh;
      font-family: var(--font-ui); font-size: 15px; line-height: 1.55;
      color: var(--ink); background: var(--ambient); background-attachment: fixed;
      display: flex;
    }

    /* Floating glass navigation */
    .sidebar {
      position: fixed; top: var(--shell-inset); left: var(--shell-inset); bottom: var(--shell-inset);
      width: var(--nav-w); display: flex; flex-direction: column; z-index: 100; overflow-y: auto;
      border: 1px solid var(--glass-edge-dark); border-radius: var(--radius-xl);
      background: var(--glass-dark); box-shadow: var(--glass-shadow-dark);
      -webkit-backdrop-filter: blur(30px) saturate(145%);
      backdrop-filter: blur(30px) saturate(145%);
      scrollbar-width: thin; scrollbar-color: rgba(255,255,255,.18) transparent;
    }
    .sidebar-brand {
      display: grid; grid-template-columns: 38px 1fr; grid-template-rows: auto auto;
      column-gap: 10px; align-items: center; padding: 22px 20px 18px;
      border-bottom: 1px solid rgba(255,255,255,.10);
    }
    .sidebar-brand::before {
      content: "S"; grid-row: 1 / 3; width: 38px; height: 38px; display: grid; place-items: center;
      border: 1px solid rgba(255,255,255,.22); border-radius: 13px; color: #fff;
      font-size: 18px; font-weight: 650; letter-spacing: -.04em;
      background: linear-gradient(145deg, rgba(255,255,255,.26), rgba(255,255,255,.08));
      box-shadow: inset 0 1px 0 rgba(255,255,255,.35), 0 10px 24px -14px rgba(0,0,0,.70);
    }
    .sidebar-brand h2 { font-size: 17px; font-weight: 650; color: #fff; letter-spacing: -.03em; }
    .sidebar-brand span { margin-top: 1px; color: rgba(255,255,255,.58); font-family: var(--font-mono); font-size: 10px; letter-spacing: .04em; }
    .sidebar-nav { flex: 1; padding: 12px; }
    .sidebar-nav a {
      display: flex; align-items: center; gap: 10px; min-height: 44px; padding: 7px 11px;
      border: 1px solid transparent; border-radius: 14px; color: rgba(255,255,255,.78);
      font-size: 13px; font-weight: 550; text-decoration: none; cursor: pointer;
      transition: color var(--dur-ui) var(--ease-out), background-color var(--dur-ui) var(--ease-out), border-color var(--dur-ui) var(--ease-out), box-shadow var(--dur-ui) var(--ease-out);
    }
    .sidebar-nav a .ico {
      width: 28px; height: 28px; display: grid; place-items: center; flex-shrink: 0;
      border-radius: 9px; background: rgba(255,255,255,.07); box-shadow: inset 0 1px 0 rgba(255,255,255,.10);
      font-size: 15px;
    }
    .sidebar-nav a.active {
      color: #fff; border-color: rgba(180,225,255,.22);
      background: linear-gradient(135deg, rgba(255,255,255,.20), rgba(255,255,255,.07));
      box-shadow: inset 0 1px 0 rgba(255,255,255,.20), 0 8px 24px -16px rgba(0,0,0,.75);
    }
    .sidebar-divider { height: 1px; margin: 8px 12px; background: linear-gradient(90deg, transparent, rgba(255,255,255,.16), transparent); }
    .sidebar-section-label {
      padding: 6px 11px 4px; color: rgba(255,255,255,.52); font-family: var(--font-mono);
      font-size: 10px; letter-spacing: .08em; text-transform: uppercase;
    }
    .sidebar-footer { padding: 12px; border-top: 1px solid rgba(255,255,255,.10); }
    .sidebar-footer .btn { width: 100%; }

    /* Main */
    .main {
      margin-left: calc(var(--shell-inset) + var(--nav-w) + 24px); margin-right: auto;
      flex: 1; min-width: 0; max-width: var(--content-max);
      padding: calc(36px + env(safe-area-inset-top)) 32px 64px;
    }
    .page-title { font-size: clamp(2rem, 3.2vw, 2.55rem); line-height: 1.04; font-weight: 650; color: var(--ink); letter-spacing: -.04em; }
    .page-subtitle { max-width: 68ch; margin-top: 8px; margin-bottom: 26px; color: var(--body); font-size: 14px; line-height: 1.65; }

    /* Liquid glass cards */
    .card {
      position: relative; isolation: isolate; overflow: hidden; margin-bottom: 18px;
      border: 1px solid var(--glass-edge); border-radius: var(--radius-lg);
      background: var(--glass-light); box-shadow: var(--glass-shadow);
      -webkit-backdrop-filter: blur(var(--glass-blur)) saturate(var(--glass-sat));
      backdrop-filter: blur(var(--glass-blur)) saturate(var(--glass-sat));
    }
    .card::before {
      content: ""; position: absolute; inset: 0; z-index: 0; pointer-events: none; border-radius: inherit;
      background:
        radial-gradient(circle at var(--glass-x, 18%) var(--glass-y, -10%), rgba(255,255,255,.88), transparent 44%),
        linear-gradient(120deg, rgba(255,255,255,.34), transparent 38%);
      opacity: .70;
    }
    .card > * { position: relative; z-index: 1; }
    .card-header { display: flex; align-items: center; gap: 9px; padding: 18px 22px 0; color: var(--ink); font-size: 15px; font-weight: 650; letter-spacing: -.015em; }
    .card-body { padding: 17px 22px 22px; }
    .card-header + .card-body { padding-top: 14px; }

    /* Panel transitions */
    .panel { display: none; }
    .panel.active { display: block; animation: panel-in var(--dur-panel) var(--ease-out) both; }
    @keyframes panel-in {
      from { opacity: 0; transform: translateY(8px) scale(.995); }
      to { opacity: 1; transform: none; }
    }

    /* Form */
    .form-group { margin-bottom: 16px; }
    .form-group:last-child { margin-bottom: 0; }
    .form-label { display: block; margin-bottom: 6px; color: var(--body); font-size: 12.5px; font-weight: 600; letter-spacing: .01em; }
    .form-input, .form-select, .form-textarea {
      width: 100%; min-height: var(--control-h); padding: 10px 14px; color: var(--ink);
      font-family: inherit; font-size: 15px; line-height: 1.45;
      border: 1px solid var(--hairline-strong); border-radius: var(--radius-sm);
      background: rgba(255,255,255,.60); box-shadow: inset 0 1px 2px rgba(30,60,90,.04);
      outline: none;
      transition: border-color var(--dur-ui) var(--ease-out), box-shadow var(--dur-ui) var(--ease-out), background-color var(--dur-ui) var(--ease-out);
    }
    .form-input::placeholder, .form-textarea::placeholder { color: #7b8b9c; opacity: 1; }
    .form-input:focus, .form-select:focus, .form-textarea:focus { border-color: var(--link); background: rgba(255,255,255,.80); box-shadow: var(--focus-ring); }
    .form-select { cursor: pointer; }
    .form-textarea { min-height: 104px; resize: vertical; line-height: 1.6; }
    .form-hint { margin-top: 6px; color: var(--mute); font-size: 12px; line-height: 1.55; }
    .form-warning { margin-bottom: 16px; padding: 11px 14px; border: 1px solid rgba(166,98,0,.20); border-radius: var(--radius-sm); background: rgba(255,184,76,.14); color: #7f4c00; font-size: 12px; line-height: 1.55; }
    .form-row { display: flex; gap: 16px; }
    .form-row .form-group { flex: 1; }

    /* Buttons */
    .btn {
      display: inline-flex; align-items: center; justify-content: center; gap: 6px;
      min-height: var(--control-h); padding: 0 18px; border: 0; border-radius: var(--radius-pill);
      font-family: inherit; font-size: 13px; font-weight: 600; line-height: 1.4; letter-spacing: .005em;
      cursor: pointer; white-space: nowrap;
      transition: transform var(--dur-press) ease-out, background-color var(--dur-ui) var(--ease-out), box-shadow var(--dur-ui) var(--ease-out), opacity var(--dur-ui) var(--ease-out);
    }
    .btn:active { transform: scale(.98); }
    .btn:disabled { opacity: .50; cursor: not-allowed; transform: none; }
    .btn-primary { color: #fff; background: linear-gradient(180deg, #0777d9, #006ed0 55%, #005bb5); box-shadow: inset 0 1px 0 rgba(255,255,255,.30), 0 10px 24px -12px rgba(0,90,170,.72); }
    .btn-secondary { color: var(--ink); background: rgba(255,255,255,.58); box-shadow: inset 0 0 0 1px var(--hairline-strong); }
    .btn-danger { color: #a92520; background: rgba(235,72,64,.12); box-shadow: inset 0 0 0 1px rgba(201,49,42,.28); }
    .btn-white { color: var(--ink); background: rgba(255,255,255,.92); box-shadow: inset 0 1px 0 #fff; }
    .btn-sm { min-height: 36px; padding: 0 12px; border-radius: var(--radius-sm); font-size: 12px; }
    .btn-block { width: 100%; justify-content: center; }
    .btn-save { width: 100%; min-height: 48px; margin-top: 2px; font-size: 14px; }
    .btn:focus-visible, .sidebar-nav a:focus-visible, .form-input:focus-visible, .form-select:focus-visible, .form-textarea:focus-visible,
    .push-channel-body input:focus-visible, .push-channel-body select:focus-visible, .push-channel-body textarea:focus-visible { outline: 3px solid rgba(10,132,255,.35); outline-offset: 2px; }

    /* Push Channel */
    .push-channel { margin-bottom: 10px; padding: 15px; border: 1px solid rgba(255,255,255,.58); border-radius: var(--radius-md); background: rgba(255,255,255,.42); transition: background-color var(--dur-ui) var(--ease-out), border-color var(--dur-ui) var(--ease-out); }
    .push-channel.enabled { border-color: rgba(10,132,255,.28); background: rgba(245,251,255,.66); }
    .push-channel-header { display: flex; align-items: center; gap: 10px; min-height: var(--control-h); margin-bottom: 10px; }
    .push-channel-header label { flex: 1; color: var(--ink); font-size: 13px; font-weight: 650; cursor: pointer; }
    .push-channel-header input[type="checkbox"] {
      -webkit-appearance: none; appearance: none; position: relative; flex: 0 0 44px;
      width: 44px; height: 26px; border-radius: 999px; background: rgba(120,138,154,.30);
      box-shadow: inset 0 0 0 1px rgba(35,70,103,.12); cursor: pointer;
      transition: background-color var(--dur-ui) var(--ease-out), box-shadow var(--dur-ui) var(--ease-out);
    }
    .push-channel-header input[type="checkbox"]::after {
      content: ""; position: absolute; top: 3px; left: 3px; width: 20px; height: 20px;
      border-radius: 50%; background: #fff; box-shadow: 0 2px 6px rgba(24,55,83,.28);
      transition: transform var(--dur-ui) var(--ease-out);
    }
    .push-channel-header input[type="checkbox"]:checked { background: #30b95c; box-shadow: inset 0 0 0 1px rgba(20,120,62,.20); }
    .push-channel-header input[type="checkbox"]:checked::after { transform: translateX(18px); }
    .push-channel-header input[type="checkbox"]:focus-visible { outline: 3px solid rgba(10,132,255,.35); outline-offset: 2px; }
    .push-channel-body { display: none; }
    .push-channel.enabled .push-channel-body { display: block; }
    .push-channel-body .form-group { margin-bottom: 14px; }
    .push-channel-body .form-group:last-child { margin-bottom: 0; }
    .push-channel-body label { display: block; margin-bottom: 6px; color: var(--body); font-size: 12px; font-weight: 600; letter-spacing: .01em; }
    .push-channel-body input, .push-channel-body select, .push-channel-body textarea {
      width: 100%; min-height: var(--control-h); padding: 10px 14px; color: var(--ink); font-family: inherit; font-size: 15px;
      border: 1px solid var(--hairline-strong); border-radius: var(--radius-sm); background: rgba(255,255,255,.66); outline: none;
      transition: border-color var(--dur-ui) var(--ease-out), box-shadow var(--dur-ui) var(--ease-out), background-color var(--dur-ui) var(--ease-out);
    }
    .push-channel-body input:focus, .push-channel-body select:focus, .push-channel-body textarea:focus { border-color: var(--link); background: rgba(255,255,255,.84); box-shadow: var(--focus-ring); }
    .push-channel-body select { cursor: pointer; }
    .push-channel-body textarea { min-height: 110px; resize: vertical; line-height: 1.6; }
    .push-type-hint { margin-top: 6px; padding: 9px 12px; border-radius: var(--radius-sm); background: rgba(236,246,255,.70); color: var(--body); font-family: var(--font-mono); font-size: 11px; line-height: 1.55; overflow-wrap: anywhere; word-break: break-word; }
    .push-test-btn { flex: 0 0 auto; }
    .push-test-result { margin: -2px 0 10px; }

    /* Forward history */
    .history-refresh { margin-left: auto; }
    .history-list { display: grid; gap: 10px; }
    .history-empty { padding: 28px 16px; border: 1px dashed var(--hairline-strong); border-radius: var(--radius-md); color: var(--mute); text-align: center; font-size: 13px; }
    .history-item { padding: 14px 15px; border: 1px solid rgba(255,255,255,.58); border-radius: var(--radius-md); background: rgba(255,255,255,.44); }
    .history-top { display: flex; align-items: flex-start; gap: 10px; }
    .history-sender { flex: 1; min-width: 0; color: var(--ink); font-size: 14px; font-weight: 650; letter-spacing: -.01em; overflow-wrap: anywhere; }
    .history-time { flex: none; color: var(--mute); font-family: var(--font-mono); font-size: 10.5px; line-height: 1.7; }
    .history-message { max-height: 11em; margin: 9px 0 11px; overflow: auto; color: var(--body); font-size: 13px; line-height: 1.65; white-space: pre-wrap; overflow-wrap: anywhere; }
    .history-channels { display: flex; flex-wrap: wrap; gap: 6px; margin: -3px 0 11px; }
    .history-channel { display: inline-flex; align-items: center; min-height: 24px; padding: 0 9px; border-radius: var(--radius-pill); font-size: 11px; font-weight: 600; }
    .history-channel.ok { color: #116c47; background: rgba(52,199,123,.14); }
    .history-channel.error { color: #a92520; background: rgba(235,72,64,.12); }
    .history-reason { margin: -2px 0 10px; color: var(--mute); font-size: 11.5px; }
    .history-footer { display: flex; align-items: center; gap: 9px; }
    .history-status { display: inline-flex; align-items: center; min-height: 24px; padding: 0 9px; border-radius: var(--radius-pill); font-size: 11px; font-weight: 650; }
    .history-status.ok { color: #116c47; background: rgba(52,199,123,.14); }
    .history-status.partial { color: #8b5800; background: rgba(255,184,76,.16); }
    .history-status.error { color: #a92520; background: rgba(235,72,64,.12); }
    .history-status.muted { color: var(--mute); background: rgba(120,138,154,.12); }
    .history-delete { margin-left: auto; }

    /* Result Boxes */
    .result-box { display: none; margin-top: 12px; padding: 10px 14px; border: 1px solid transparent; border-radius: 14px; font-size: 12px; font-weight: 500; line-height: 1.55; }
    .result-success { display: block; color: #116c47; background: rgba(52,199,123,.14); border-color: rgba(34,150,94,.20); }
    .result-error { display: block; color: #a92520; background: rgba(235,72,64,.12); border-color: rgba(201,49,42,.20); }
    .result-loading { display: block; color: #8b5800; background: rgba(255,184,76,.16); border-color: rgba(166,98,0,.18); }
    .result-info { display: block; color: #0969b0; background: rgba(10,132,255,.10); border-color: rgba(10,132,255,.18); }
    .status-live { color: var(--mute); font-weight: 650; }
    .mono-mark { color: var(--mute); font-weight: 650; }
    .info-table { width: 100%; border-collapse: collapse; margin-top: 2px; font-size: 13px; table-layout: fixed; }
    .info-table td { padding: 10px 8px; border-bottom: 1px solid var(--hairline); vertical-align: top; word-break: break-word; }
    .info-table tr:last-child td { border-bottom: 0; }
    .info-table td:first-child { font-weight: 500; color: var(--body); width: 40%; }

    /* Overview */
    .overview-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    .overview-item { padding: 15px; border: 1px solid rgba(255,255,255,.54); border-radius: var(--radius-md); background: rgba(255,255,255,.46); box-shadow: inset 0 1px 0 rgba(255,255,255,.60); }
    .overview-item .label { margin-bottom: 5px; color: var(--mute); font-family: var(--font-mono); font-size: 10px; letter-spacing: .06em; text-transform: uppercase; }
    .overview-item .value { color: var(--ink); font-size: 17px; font-weight: 650; font-variant-numeric: tabular-nums; letter-spacing: -.02em; overflow-wrap: anywhere; }
    .net-switch {
      display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 4px; padding: 4px;
      border: 1px solid var(--hairline); border-radius: 16px;
      background: rgba(242,247,252,.64); box-shadow: inset 0 1px 2px rgba(30,60,90,.04);
    }
    .net-option {
      min-height: 40px; padding: 0 8px; border: 0; border-radius: 12px;
      color: var(--body); background: transparent; font-family: inherit; font-size: 12.5px;
      font-weight: 600; line-height: 1.25; cursor: pointer; white-space: nowrap;
      transition: color var(--dur-ui) var(--ease-out), background-color var(--dur-ui) var(--ease-out), box-shadow var(--dur-ui) var(--ease-out), transform var(--dur-press) ease-out;
    }
    .net-option:active { transform: scale(.985); }
    .net-option.active {
      color: #fff; background: linear-gradient(180deg, #273747, #172433);
      box-shadow: inset 0 1px 0 rgba(255,255,255,.16), 0 6px 16px -10px rgba(5,20,35,.72);
    }
    .net-option:focus-visible { outline: 3px solid rgba(10,132,255,.35); outline-offset: 2px; }
    .net-option:disabled { cursor: wait; opacity: .58; }
    .net-mode-row { display: flex; align-items: center; gap: 8px; margin-top: 14px; color: var(--mute); font-size: 12px; }
    .net-mode-row .net-current {
      color: var(--ink); font-family: var(--font-mono); font-size: 11.5px; font-weight: 600;
      letter-spacing: -.01em;
    }
    .wifi-current {
      display: flex; align-items: center; justify-content: space-between; gap: 14px;
      margin-bottom: 16px; padding: 12px 14px; border: 1px solid rgba(255,255,255,.58);
      border-radius: var(--radius-md); background: rgba(255,255,255,.44);
    }
    .wifi-current-main { display: flex; align-items: center; gap: 9px; min-width: 0; color: var(--ink); font-size: 13px; font-weight: 650; }
    .wifi-dot {
      width: 8px; height: 8px; flex: none; border-radius: 50%;
      background: #52667a; box-shadow: 0 0 0 3px rgba(82,102,122,.12);
    }
    .wifi-meta { display: flex; flex-wrap: wrap; justify-content: flex-end; gap: 4px 12px; color: var(--mute); font-family: var(--font-mono); font-size: 10.5px; }
    .wifi-picker { display: flex; align-items: stretch; gap: 8px; }
    .wifi-picker .form-select { flex: 1; min-width: 0; }
    .wifi-picker .btn { flex: 0 0 auto; }
    .wifi-actions { display: flex; flex-wrap: wrap; gap: 8px; }
    .wifi-actions .btn { flex: 1 1 170px; }
    .ap-switch-row { display: flex; align-items: center; gap: 10px; margin-bottom: 16px; color: var(--ink); font-size: 13px; font-weight: 650; cursor: pointer; }
    .ap-switch-row input[type="checkbox"] {
      -webkit-appearance: none; appearance: none; position: relative; flex: 0 0 44px;
      width: 44px; height: 26px; border-radius: 999px; background: rgba(120,138,154,.30);
      box-shadow: inset 0 0 0 1px rgba(35,70,103,.12); cursor: pointer;
      transition: background-color var(--dur-ui) var(--ease-out), box-shadow var(--dur-ui) var(--ease-out);
    }
    .ap-switch-row input[type="checkbox"]::after {
      content: ""; position: absolute; top: 3px; left: 3px; width: 20px; height: 20px;
      border-radius: 50%; background: #fff; box-shadow: 0 2px 6px rgba(24,55,83,.28);
      transition: transform var(--dur-ui) var(--ease-out);
    }
    .ap-switch-row input[type="checkbox"]:checked { background: #30b95c; box-shadow: inset 0 0 0 1px rgba(20,120,62,.20); }
    .ap-switch-row input[type="checkbox"]:checked::after { transform: translateX(18px); }
    .ap-switch-row input[type="checkbox"]:focus-visible { outline: 3px solid rgba(10,132,255,.35); outline-offset: 2px; }
    .ap-fixed {
      display: flex; align-items: center; min-height: var(--control-h); padding: 0 14px;
      border: 1px solid var(--hairline); border-radius: var(--radius-sm);
      color: var(--body); background: rgba(242,247,252,.64); font-family: var(--font-mono); font-size: 12px;
    }
    .modal-backdrop {
      position: fixed; inset: 0; z-index: 500; display: flex; align-items: center; justify-content: center;
      padding: 20px; background: rgba(15,26,40,.34);
      -webkit-backdrop-filter: blur(10px) saturate(130%); backdrop-filter: blur(10px) saturate(130%);
    }
    .modal-backdrop[hidden] { display: none; }
    .modal-card {
      width: min(100%, 420px); padding: 24px; border: 1px solid var(--glass-edge);
      border-radius: var(--radius-lg); background: var(--glass-light); box-shadow: var(--glass-shadow);
      -webkit-backdrop-filter: blur(var(--glass-blur)) saturate(var(--glass-sat));
      backdrop-filter: blur(var(--glass-blur)) saturate(var(--glass-sat));
    }
    .modal-card h2 { margin: 0 0 10px; color: var(--ink); font-size: 20px; letter-spacing: -.025em; }
    .modal-card p { margin: 0; color: var(--body); font-size: 13px; line-height: 1.7; }
    .modal-actions { display: flex; justify-content: flex-end; gap: 8px; margin-top: 20px; }

    /* Tools */
    .btn-row { display: grid; grid-template-columns: repeat(auto-fit, minmax(132px, 1fr)); gap: 8px; }
    .btn-row .btn { width: 100%; }
    .btn-row + .btn-row { margin-top: 8px; }
    #atLog, #logView {
      min-height: 130px; max-height: 260px; overflow-y: auto; margin-bottom: 10px; padding: 12px 14px;
      border: 1px solid rgba(255,255,255,.12); border-radius: var(--radius-md);
      background: linear-gradient(160deg, rgba(13,24,38,.94), rgba(18,33,51,.90));
      color: #d9f7ef; font-family: var(--font-mono); font-size: 12px; line-height: 1.6;
      box-shadow: inset 0 1px 0 rgba(255,255,255,.10), 0 18px 36px -26px rgba(3,14,28,.72);
      white-space: pre-wrap; word-break: break-all;
    }
    #logView { min-height: 300px; max-height: 60vh; }
    .at-bar { display: flex; gap: 8px; }
    .at-bar input { flex: 1; min-width: 0; font-family: var(--font-mono); }
    .at-bar .btn { min-width: 66px; }
    #panel-log .btn-row label { display: inline-flex; align-items: center; gap: 8px; min-height: var(--control-h); padding: 0 4px; }
    #logAuto {
      -webkit-appearance: none; appearance: none; position: relative; flex: 0 0 44px;
      width: 44px; height: 26px; border-radius: 999px; background: rgba(120,138,154,.30);
      box-shadow: inset 0 0 0 1px rgba(35,70,103,.12); cursor: pointer;
      transition: background-color var(--dur-ui) var(--ease-out), box-shadow var(--dur-ui) var(--ease-out);
    }
    #logAuto::after {
      content: ""; position: absolute; top: 3px; left: 3px; width: 20px; height: 20px;
      border-radius: 50%; background: #fff; box-shadow: 0 2px 6px rgba(24,55,83,.28);
      transition: transform var(--dur-ui) var(--ease-out);
    }
    #logAuto:checked { background: #30b95c; box-shadow: inset 0 0 0 1px rgba(20,120,62,.20); }
    #logAuto:checked::after { transform: translateX(18px); }

    /* Responsive */
    @media (hover: hover) {
      .sidebar-nav a:not(.active):hover { color: #fff; background: rgba(255,255,255,.08); }
      .btn-primary:hover { background: linear-gradient(180deg, #0873d1, #0066c2 55%, #0053a8); }
      .btn-secondary:hover, .btn-white:hover { background: rgba(255,255,255,.82); }
      .btn-danger:hover { background: rgba(235,72,64,.17); }
    }
    @media (max-width: 820px) {
      :root { --glass-blur: 22px; }
      body { display: block; background-attachment: scroll; }
      .sidebar {
        top: auto; left: max(12px, env(safe-area-inset-left)); right: max(12px, env(safe-area-inset-right));
        bottom: calc(10px + env(safe-area-inset-bottom)); width: auto; height: 72px;
        border-radius: 24px; overflow: visible; display: block;
      }
      .sidebar-brand, .sidebar-section-label, .sidebar-divider, .sidebar-footer { display: none; }
      .sidebar-nav {
        display: flex; align-items: center; gap: 4px; height: 100%; padding: 6px;
        overflow-x: auto; overflow-y: hidden; scroll-padding-inline: 16px;
        scroll-snap-type: x proximity; scrollbar-width: none;
      }
      .sidebar-nav::-webkit-scrollbar { display: none; }
      .sidebar::after {
        content: ""; position: absolute; top: 8px; right: 8px; bottom: 8px; width: 30px;
        border-radius: 18px; pointer-events: none;
        background: linear-gradient(90deg, transparent, rgba(9,20,34,.58));
      }
      .sidebar-nav a {
        flex: 0 0 64px; min-height: 58px; flex-direction: column; justify-content: center;
        gap: 2px; margin: 0; padding: 4px; border-radius: 16px; font-size: 10px; white-space: nowrap;
        scroll-snap-align: center;
      }
      .sidebar-nav a .ico { width: 24px; height: 24px; font-size: 16px; }
      .main { margin: 0; max-width: none; padding: calc(24px + env(safe-area-inset-top)) max(16px, env(safe-area-inset-left)) calc(104px + env(safe-area-inset-bottom)) max(16px, env(safe-area-inset-right)); }
      .page-title { font-size: 30px; }
      .page-subtitle { margin-bottom: 20px; }
      .card { margin-bottom: 14px; border-radius: 22px; }
      .card-header { padding: 18px 18px 0; }
      .card-body { padding: 16px 18px 20px; }
      .card-header + .card-body { padding-top: 13px; }
      .overview-grid { grid-template-columns: 1fr; }
      .btn-sm { min-height: 44px; }
      #logView { min-height: 260px; max-height: 56vh; }
    }
    @media (max-width: 700px) {
      .form-row { display: block; }
      .form-row .form-group { margin-bottom: 16px; }
      .form-row .form-group:last-child { margin-bottom: 0; }
      .history-top { flex-direction: column; gap: 4px; }
      .history-time { font-size: 11px; }
      .net-option { min-height: 44px; padding: 0 4px; font-size: 11.5px; }
      .wifi-current { align-items: flex-start; flex-direction: column; gap: 7px; }
      .wifi-meta { justify-content: flex-start; }
      .modal-actions { flex-direction: column-reverse; }
      .modal-actions .btn { width: 100%; }
      .form-input, .form-select, .form-textarea,
      .push-channel-body input, .push-channel-body select, .push-channel-body textarea { font-size: 16px; }
    }
    @media (prefers-reduced-motion: reduce) {
      .panel.active { animation: panel-fade 160ms ease both; }
      .btn, .sidebar-nav a, .push-channel, .push-channel-header input[type="checkbox"],
      .push-channel-header input[type="checkbox"]::after { transition-duration: .01ms; }
      .btn:active { transform: none; }
      #logAuto, #logAuto::after { transition-duration: .01ms; }
    }
    @keyframes panel-fade { from { opacity: 0; } to { opacity: 1; } }
    @media (prefers-reduced-transparency: reduce) {
      .sidebar { background: #15243a; -webkit-backdrop-filter: none; backdrop-filter: none; }
      .card { background: rgba(255,255,255,.96); -webkit-backdrop-filter: none; backdrop-filter: none; }
      .card::before { opacity: .22; }
      .overview-item, .push-channel { background: rgba(255,255,255,.86); }
    }
    @media (prefers-contrast: more) {
      :root { --ink: #081421; --body: #1e3245; --mute: #3e5265; --hairline: rgba(20,50,75,.24); --hairline-strong: rgba(20,50,75,.42); }
      .card, .overview-item, .push-channel { background: rgba(255,255,255,.97); }
      .form-input, .form-select, .form-textarea, .push-channel-body input, .push-channel-body select, .push-channel-body textarea { background: #fff; }
    }
    @media (forced-colors: active) {
      .sidebar, .card, .overview-item, .push-channel, .form-input, .form-select, .form-textarea,
      .push-channel-body input, .push-channel-body select, .push-channel-body textarea {
        background: Canvas; color: CanvasText; border: 1px solid CanvasText;
        -webkit-backdrop-filter: none; backdrop-filter: none;
      }
      .card::before { display: none; }
      .sidebar-nav a { color: LinkText; }
      .sidebar-nav a.active { background: Highlight; color: HighlightText; border-color: Highlight; }
      .btn { border: 1px solid ButtonText; background: ButtonFace; color: ButtonText; }
      .push-channel-header input[type="checkbox"], #logAuto {
        -webkit-appearance: auto; appearance: auto; width: auto; height: auto;
        border-radius: 0; background: none; box-shadow: none;
      }
      .push-channel-header input[type="checkbox"]::after, #logAuto::after { display: none; }
      #atLog, #logView { background: Canvas; color: CanvasText; border: 1px solid CanvasText; }
      .status-live { color: CanvasText; }
    }
    @supports not ((backdrop-filter: blur(1px)) or (-webkit-backdrop-filter: blur(1px))) {
      .sidebar { background: rgba(17,31,48,.97); }
      .card { background: rgba(255,255,255,.94); }
      .card::before { opacity: .30; }
    }
  </style>
</head>
<body>
  <svg class="icon-sprite" aria-hidden="true" focusable="false">
    <symbol id="i-home" viewBox="0 0 24 24">
      <path d="M3 10.5 12 3l9 7.5"/><path d="M5 9.5V21h14V9.5"/><path d="M9 21v-6h6v6"/>
    </symbol>
    <symbol id="i-lock" viewBox="0 0 24 24">
      <rect x="5" y="10" width="14" height="11" rx="3"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/>
    </symbol>
    <symbol id="i-clock" viewBox="0 0 24 24">
      <circle cx="12" cy="12" r="9"/><path d="M12 7v5l3.5 2"/>
    </symbol>
    <symbol id="i-link" viewBox="0 0 24 24">
      <path d="M10 13a5 5 0 0 0 7.1 0l2-2A5 5 0 0 0 12 3.9l-1.1 1.1"/>
      <path d="M14 11a5 5 0 0 0-7.1 0l-2 2A5 5 0 0 0 12 20.1l1.1-1.1"/>
    </symbol>
    <symbol id="i-user" viewBox="0 0 24 24">
      <circle cx="12" cy="8" r="4"/><path d="M4 21a8 8 0 0 1 16 0"/>
    </symbol>
    <symbol id="i-send" viewBox="0 0 24 24">
      <path d="m3 11 18-8-8 18-2.5-7.5L3 11Z"/><path d="m10.5 13.5 10.5-10.5"/>
    </symbol>
    <symbol id="i-pulse" viewBox="0 0 24 24">
      <path d="M3 12h3l2-6 3 12 3-9 2 3h5"/>
    </symbol>
    <symbol id="i-bars" viewBox="0 0 24 24">
      <path d="M5 20v-6"/><path d="M10 20V9"/><path d="M15 20V5"/><path d="M20 20v-9"/>
    </symbol>
    <symbol id="i-globe" viewBox="0 0 24 24">
      <circle cx="12" cy="12" r="9"/><path d="M3 12h18"/>
      <path d="M12 3c3 3.2 3 14.8 0 18"/><path d="M12 3c-3 3.2-3 14.8 0 18"/>
    </symbol>
    <symbol id="i-antenna" viewBox="0 0 24 24">
      <path d="M12 21V8"/><circle cx="12" cy="5" r="2"/>
      <path d="M8.5 11a5 5 0 0 1 7 0"/><path d="M5 14a10 10 0 0 1 14 0"/>
    </symbol>
    <symbol id="i-wifi" viewBox="0 0 24 24">
      <path d="M3 8.8a14 14 0 0 1 18 0"/><path d="M6.5 12.5a9 9 0 0 1 11 0"/>
      <path d="M10 16.3a4.5 4.5 0 0 1 4 0"/><circle cx="12" cy="20" r=".6" fill="currentColor" stroke="none"/>
    </symbol>
    <symbol id="i-sliders" viewBox="0 0 24 24">
      <path d="M4 7h16"/><path d="M4 17h16"/><circle cx="9" cy="7" r="2"/><circle cx="15" cy="17" r="2"/>
    </symbol>
    <symbol id="i-refresh" viewBox="0 0 24 24">
      <path d="M20 11a8 8 0 1 0-2.3 6.7"/><path d="M20 5v6h-6"/>
    </symbol>
    <symbol id="i-ban" viewBox="0 0 24 24">
      <circle cx="12" cy="12" r="9"/><path d="m6 6 12 12"/>
    </symbol>
    <symbol id="i-search" viewBox="0 0 24 24">
      <circle cx="11" cy="11" r="7"/><path d="m16.5 16.5 4 4"/>
    </symbol>
    <symbol id="i-arrow-end" viewBox="0 0 24 24">
      <path d="M4 12h16"/><path d="m15 7 5 5-5 5"/>
    </symbol>
    <symbol id="i-message" viewBox="0 0 24 24">
      <path d="M21 11.5a8.5 8.5 0 0 1-8.5 8.5H6l-3 2V11.5A8.5 8.5 0 0 1 11.5 3h1A8.5 8.5 0 0 1 21 11.5Z"/>
    </symbol>
    <symbol id="i-plane" viewBox="0 0 24 24">
      <path d="M2 16h20"/><path d="M12 4 9 16"/><path d="m12 4 3 12"/><path d="M8 12h8"/>
    </symbol>
    <symbol id="i-terminal" viewBox="0 0 24 24">
      <rect x="3" y="4" width="18" height="16" rx="3"/><path d="m7 9 3 3-3 3"/><path d="M13 15h4"/>
    </symbol>
    <symbol id="i-list" viewBox="0 0 24 24">
      <path d="M9 6h11"/><path d="M9 12h11"/><path d="M9 18h11"/><path d="M4 6h.01"/><path d="M4 12h.01"/><path d="M4 18h.01"/>
    </symbol>
    <symbol id="i-key" viewBox="0 0 24 24">
      <circle cx="8" cy="15" r="4"/><path d="m11 12 9-9"/><path d="m17 4 3 3"/><path d="m14 7 3 3"/>
    </symbol>
  </svg>
  <aside class="sidebar">
    <div class="sidebar-brand">
      <h2>SMS FWD</h2>
      <span>短信转发器</span>
    </div>
    <nav class="sidebar-nav" aria-label="主导航">
      <div class="sidebar-section-label">配置</div>
      <a data-panel="overview" class="active" aria-current="page"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-home"></use></svg></span> <span>系统设置</span></a>
      <a data-panel="history"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-clock"></use></svg></span> <span>短信记录</span></a>
      <a data-panel="push"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-link"></use></svg></span> <span>推送通道</span></a>
      <a data-panel="operator"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-antenna"></use></svg></span> <span>选网配置</span></a>
      <a data-panel="admin"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-user"></use></svg></span> <span>管理员 &amp; 黑名单</span></a>
      <div class="sidebar-divider"></div>
      <div class="sidebar-section-label">工具</div>
      <a data-panel="sendsms"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-send"></use></svg></span> <span>发送短信</span></a>
      <a data-panel="diagnose"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-pulse"></use></svg></span> <span>模组诊断</span></a>
      <a data-panel="network"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-globe"></use></svg></span> <span>网络测试</span></a>
      <a data-panel="modem"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-bars"></use></svg></span> <span>模组控制</span></a>
      <a data-panel="atterm"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-terminal"></use></svg></span> <span>AT 终端</span></a>
      <a data-panel="log"><span class="ico" aria-hidden="true"><svg class="ui-icon"><use href="#i-list"></use></svg></span> <span>系统日志</span></a>
    </nav>
    <div class="sidebar-footer">
      <button class="btn btn-white btn-sm btn-block" onclick="switchPanel('account')"><span>修改密码</span><svg class="ui-icon" aria-hidden="true"><use href="#i-key"></use></svg></button>
    </div>
  </aside>

  <main class="main">

    <!-- ===== Overview ===== -->
    <div class="panel active" id="panel-overview">
      <h1 class="page-title">系统设置</h1>
      <p class="page-subtitle">设备状态与基本信息</p>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-antenna"></use></svg> 设备信息</div>
        <div class="card-body">
          <div class="overview-grid">
            <div class="overview-item"><div class="label">Device IP</div><div class="value" id="ovIp">%IP%</div></div>
            <div class="overview-item"><div class="label">WiFi SSID</div><div class="value" id="ovSsid">%WIFI_SSID%</div></div>
            <div class="overview-item"><div class="label">Free Heap</div><div class="value" id="ovHeap">%FREE_HEAP%</div></div>
            <div class="overview-item"><div class="label">Uptime</div><div class="value" id="ovUptime">%UPTIME%</div></div>
          </div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-wifi"></use></svg> WiFi 连接</div>
        <div class="card-body">
          <div class="wifi-current">
            <div class="wifi-current-main"><span class="wifi-dot" aria-hidden="true"></span><span id="wifiStatusText">%WIFI_STATUS_TEXT%</span><span id="wifiCurrentSsid">%WIFI_SSID%</span></div>
            <div class="wifi-meta"><span id="wifiCurrentIp">%IP%</span><span id="wifiCurrentRssi">%WIFI_RSSI_TEXT%</span></div>
          </div>
          <div class="form-group">
            <label class="form-label" for="wifiSsid">WiFi 网络</label>
            <div class="wifi-picker">
              <select class="form-select" id="wifiSsid" aria-label="WiFi 网络">
                <option value="%WIFI_SAVED_SSID%" selected>%WIFI_SAVED_SSID%</option>
              </select>
              <button type="button" class="btn btn-secondary" id="wifiScanBtn" onclick="scanWifi()">扫描</button>
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label" for="wifiAuth">加密方式</label>
              <select class="form-select" id="wifiAuth" aria-label="WiFi 加密方式">
                <option value="0">自动</option>
                <option value="1">开放（无密码）</option>
                <option value="2">WPA / WPA2</option>
                <option value="3">WPA2</option>
                <option value="4">WPA3</option>
              </select>
            </div>
            <div class="form-group">
              <label class="form-label" for="wifiPass">WiFi 密码</label>
              <input class="form-input" type="password" id="wifiPass" name="wifiPass" placeholder="留空且 SSID 不变时沿用已保存密码" autocomplete="new-password">
            </div>
          </div>
          <div class="wifi-actions">
            <button type="button" class="btn btn-primary" onclick="saveWifi(true)">保存并连接</button>
            <button type="button" class="btn btn-secondary" onclick="saveWifi(false)">仅保存</button>
          </div>
          <p class="form-hint">扫描会短暂影响 WiFi；切换网络后设备 IP 可能变化。开放网络无需填写密码。</p>
          <div class="result-box" id="wifiResult" aria-live="polite"></div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-globe"></use></svg> 推送网络</div>
        <div class="card-body">
          <div class="net-switch" role="group" aria-label="推送网络模式">
            <button type="button" class="net-option" data-push-net="0" aria-pressed="false" onclick="setPushNetMode(0)">WiFi</button>
            <button type="button" class="net-option" data-push-net="1" aria-pressed="false" onclick="setPushNetMode(1)">4G 数据</button>
            <button type="button" class="net-option" data-push-net="2" aria-pressed="false" onclick="setPushNetMode(2)">WiFi 优先 · 数据兜底</button>
          </div>
          <div class="net-mode-row"><span>当前通道</span><span class="net-current" id="pushNetCurrent">读取中</span></div>
          <p class="form-hint" id="pushNetHint">选择短信转发与测试推送使用的网络。</p>
          <div class="result-box" id="pushNetResult" aria-live="polite"></div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-wifi"></use></svg> 热点配置</div>
        <div class="card-body">
          <div class="wifi-current">
            <div class="wifi-current-main"><span class="wifi-dot" aria-hidden="true"></span><span>热点状态</span><span id="apStatusText">%AP_STATUS_TEXT%</span></div>
            <div class="wifi-meta"><span id="apIp">%AP_IP%</span><span><span id="apClients">%AP_CLIENTS%</span> 个设备</span></div>
          </div>
          <div class="form-warning" id="apPassWarning" style="display:none;">当前仍使用默认热点密码 <strong>%AP_DEFAULT_PASSWORD%</strong>，请尽快修改。</div>
          <label class="ap-switch-row" for="apEnabled"><input type="checkbox" id="apEnabled" name="apEnabled"%AP_ENABLED_CHECKED%><span>启用热点</span></label>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label" for="apSsid">热点 SSID</label>
              <input class="form-input" type="text" id="apSsid" name="apSsid" value="%AP_SSID%" maxlength="32" placeholder="%AP_DEFAULT_SSID%">
            </div>
            <div class="form-group">
              <label class="form-label" for="apPass">热点密码</label>
              <input class="form-input" type="password" id="apPass" name="apPass" placeholder="留空保持当前密码（默认 %AP_DEFAULT_PASSWORD%）" autocomplete="new-password">
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">加密方式</label>
            <div class="ap-fixed">WPA2-AES（默认，固定）</div>
          </div>
          <div class="wifi-actions">
            <button type="button" class="btn btn-primary" onclick="saveApConfig()">保存并应用</button>
          </div>
          <p class="form-hint">默认热点名称为 MAC 末 4 位 + “短信转发”；首次使用请修改默认密码。关闭热点后请通过已保存的 WiFi 连接管理设备。</p>
          <div class="result-box" id="apResult" aria-live="polite"></div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-sliders"></use></svg> 配置状态</div>
        <div class="card-body">
          <table class="info-table">
            <tr><td>模组状态</td><td id="cfgModem">%MODEM_CHECK%</td></tr>
            <tr><td>短信记录</td><td id="cfgHistory">%HISTORY_COUNT% 条</td></tr>
            <tr><td>推送通道</td><td id="cfgPush">%PUSH_COUNT% 个已启用</td></tr>
            <tr><td>管理员号码</td><td>%ADMIN_PHONE%</td></tr>
          </table>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-refresh"></use></svg> 系统控制</div>
        <div class="card-body">
          <button class="btn btn-danger" onclick="systemRestart()">重启系统</button>
          <p class="form-hint">重启整个设备（ESP32 + 模组），期间无法接收短信和访问网页，约需 1 分钟恢复</p>
          <div class="result-box" id="sysRestartResult"></div>
        </div>
      </div>
    </div>

    <!-- ===== Account ===== -->
    <div class="panel" id="panel-account">
      <h1 class="page-title">修改密码</h1>
      <p class="page-subtitle">修改 Web 管理界面的登录凭据</p>
      <form action="/save" method="POST" id="mainForm">
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-lock"></use></svg> 登录凭据</div>
        <div class="card-body">
          <div class="form-warning">首次使用请立即修改默认密码！默认: )rawliteral" DEFAULT_WEB_USER " / " DEFAULT_WEB_PASS R"rawliteral(</div>
          <div class="form-row">
            <div class="form-group"><label class="form-label">管理账号</label><input class="form-input" type="text" name="webUser" value="%WEB_USER%" placeholder="admin"></div>
            <div class="form-group"><label class="form-label">管理密码</label><input class="form-input" type="password" name="webPass" value="%WEB_PASS%" placeholder="设置复杂密码"></div>
          </div>
        </div>
      </div>
      <button type="submit" class="btn btn-primary btn-block btn-save">保存配置</button>
      </form>
    </div>

    <!-- ===== SMS History ===== -->
    <div class="panel" id="panel-history">
      <h1 class="page-title">短信记录</h1>
      <p class="page-subtitle">最近 10 条收到的短信（包括黑名单拦截），下面列出实际尝试的推送通道及成功状态</p>
      <div class="card">
        <div class="card-header">
          <svg class="ui-icon" aria-hidden="true"><use href="#i-clock"></use></svg>
          <span>最近记录</span>
          <button type="button" class="btn btn-secondary btn-sm history-refresh" onclick="loadHistory()">刷新</button>
        </div>
        <div class="card-body">
          <div id="historyList" class="history-list" aria-live="polite">加载中...</div>
        </div>
      </div>
    </div>

    <!-- ===== Push Channels ===== -->
    <div class="panel" id="panel-push">
      <h1 class="page-title">推送通道</h1>
      <p class="page-subtitle">最多 5 个独立推送通道；每个通道可用当前表单参数单独测试，不会自动保存配置</p>
      <form action="/save" method="POST" id="mainForm3">
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-link"></use></svg> 通道配置</div>
        <div class="card-body">
          %PUSH_CHANNELS%
        </div>
      </div>
      <button type="submit" class="btn btn-primary btn-block btn-save">保存配置</button>
      </form>
    </div>

    <!-- ===== Operator selection ===== -->
    <div class="panel" id="panel-operator">
      <h1 class="page-title">选网配置</h1>
      <p class="page-subtitle">默认自动选网；手动模式按 PLMN（MCC + MNC）指定 LTE 运营商</p>
      <form action="/save" method="POST" id="mainFormOperator">
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-antenna"></use></svg> 运营商选择</div>
        <div class="card-body">
          <div class="form-group">
            <label class="form-label" for="operatorMode">选网模式</label>
            <select class="form-select" id="operatorMode" name="operatorMode" onchange="toggleOperatorFields()">
              <option value="0"%OP_MODE_AUTO%>自动选择</option>
              <option value="1"%OP_MODE_MANUAL%>手动指定</option>
            </select>
          </div>
          <div class="form-group" id="manualOperatorFields">
            <label class="form-label" for="operatorCode">PLMN（MCC + MNC）</label>
            <input class="form-input" type="text" id="operatorCode" name="operatorCode" value="%OPERATOR_CODE%" inputmode="numeric" pattern="[0-9]{5,6}" maxlength="6" placeholder="例如 46000">
            <p class="form-hint">手动模式仅按 PLMN 选择 LTE 网络；保存后设备会自动重启应用，默认仍为自动选网。</p>
          </div>
        </div>
      </div>
      <button type="submit" class="btn btn-primary btn-block btn-save">保存并应用</button>
      </form>
    </div>

    <!-- ===== Admin & Blacklist ===== -->
    <div class="panel" id="panel-admin">
      <h1 class="page-title">管理员 &amp; 黑名单</h1>
      <p class="page-subtitle">远程控制权限与短信过滤</p>
      <form action="/save" method="POST" id="mainForm4">
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-user"></use></svg> 管理员手机号</div>
        <div class="card-body">
          <div class="form-group">
            <label class="form-label" for="adminPhone">管理员手机号</label>
            <input class="form-input" type="text" id="adminPhone" name="adminPhone" value="%ADMIN_PHONE%" placeholder="13800138000">
            <p class="form-hint">此号码可通过短信发送远程指令（SMS:号码:内容 发短信、RESET 重启）</p>
          </div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-ban"></use></svg> 精确号码黑名单</div>
        <div class="card-body">
          <div class="form-group">
            <textarea class="form-textarea" name="numberBlackList" aria-label="精确号码黑名单" rows="4" placeholder="每行一个完整号码">%NUMBER_BLACK_LIST%</textarea>
            <p class="form-hint">号码完全匹配时拦截；空格、横线、括号和 +86 前缀会自动归一化</p>
          </div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-search"></use></svg> 号码前缀黑名单</div>
        <div class="card-body">
          <div class="form-group">
            <textarea class="form-textarea" name="numberPrefixBlackList" aria-label="号码前缀黑名单" rows="4" placeholder="每行一个号码前缀，例如 1069">%NUMBER_PREFIX_BLACK_LIST%</textarea>
            <p class="form-hint">号码以任意一行内容开头时拦截</p>
          </div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-arrow-end"></use></svg> 号码后缀黑名单</div>
        <div class="card-body">
          <div class="form-group">
            <textarea class="form-textarea" name="numberSuffixBlackList" aria-label="号码后缀黑名单" rows="4" placeholder="每行一个号码后缀，例如 95555">%NUMBER_SUFFIX_BLACK_LIST%</textarea>
            <p class="form-hint">号码以任意一行内容结尾时拦截</p>
          </div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-message"></use></svg> 内容关键词黑名单</div>
        <div class="card-body">
          <div class="form-group">
            <textarea class="form-textarea" name="contentBlackList" aria-label="内容关键词黑名单" rows="5" placeholder="每行一个关键词，例如：&#10;贷款&#10;退订">%CONTENT_BLACK_LIST%</textarea>
            <p class="form-hint">多个关键词是“或”关系：短信正文包含其中任意一个关键词即拦截</p>
          </div>
        </div>
      </div>
      <button type="submit" class="btn btn-primary btn-block btn-save">保存配置</button>
      </form>
    </div>

    <!-- ===== Send SMS ===== -->
    <div class="panel" id="panel-sendsms">
      <h1 class="page-title">发送短信</h1>
      <p class="page-subtitle">通过模组直接发送短信</p>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-send"></use></svg> 新建短信</div>
        <div class="card-body">
          <form action="/sendsms" method="POST" target="_self">
            <div class="form-group"><label class="form-label">目标号码</label><input class="form-input" type="text" name="phone" placeholder="13800138000" required></div>
            <div class="form-group"><label class="form-label">短信内容</label><textarea class="form-textarea" name="content" placeholder="输入短信内容..." required oninput="updateCount(this)"></textarea><p class="form-hint">已输入 <span id="charCount">0</span> 字符</p></div>
            <button type="submit" class="btn btn-primary" style="padding:9px 18px;">发送短信</button>
          </form>
        </div>
      </div>
    </div>

    <!-- ===== Diagnostics ===== -->
    <div class="panel" id="panel-diagnose">
      <h1 class="page-title">模组诊断</h1>
      <p class="page-subtitle">查询模组状态、SIM 卡与网络信息</p>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-pulse"></use></svg> 查询</div>
        <div class="card-body">
          <div class="btn-row"><button class="btn btn-secondary" onclick="queryInfo('ati')">固件信息</button><button class="btn btn-secondary" onclick="queryInfo('signal')">信号质量</button></div>
          <div class="btn-row"><button class="btn btn-secondary" onclick="queryInfo('siminfo')">SIM 卡信息</button><button class="btn btn-secondary" onclick="queryInfo('network')">网络状态</button><button class="btn btn-secondary" onclick="queryInfo('wifi')">WiFi 状态</button></div>
          <div class="result-box" id="queryResult"></div>
        </div>
      </div>
    </div>

    <!-- ===== Network Test ===== -->
    <div class="panel" id="panel-network">
      <h1 class="page-title">网络测试</h1>
      <p class="page-subtitle">通过模组数据连接测试网络连通性</p>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-globe"></use></svg> Ping</div>
        <div class="card-body">
          <button class="btn btn-secondary" id="pingBtn" onclick="confirmPing()">Ping 8.8.8.8</button>
          <p class="form-hint">通过模组执行 Ping，消耗极少流量</p>
          <div class="result-box" id="pingResult"></div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-wifi"></use></svg> WiFi 控制</div>
        <div class="card-body">
          <button class="btn btn-danger" onclick="wifiRestart()">重启 WiFi</button>
          <p class="form-hint">断开当前 WiFi 连接并重新连接</p>
          <div class="result-box" id="wifiResult"></div>
        </div>
      </div>
    </div>

    <!-- ===== Modem Control ===== -->
    <div class="panel" id="panel-modem">
      <h1 class="page-title">模组控制</h1>
      <p class="page-subtitle">模组重启、飞行模式、信号查询等操作</p>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-refresh"></use></svg> 模组重启</div>
        <div class="card-body">
          <div class="btn-row"><button class="btn btn-danger" onclick="modemAction('restart')">软重启 (AT+CFUN)</button><button class="btn btn-danger" onclick="modemAction('hardreset')">硬重启 (EN引脚)</button></div>
          <p class="form-hint">软重启发送 AT+CFUN=1,1 指令（15s 超时）；硬重启通过 EN 引脚断电后重新上电</p>
          <div class="result-box" id="modemRstResult"></div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-bars"></use></svg> 信号查询</div>
        <div class="card-body">
          <div class="btn-row"><button class="btn btn-primary" onclick="modemAction('signal')">查询信号强度</button><button class="btn btn-primary" onclick="modemAction('operator')">查询运营商</button><button class="btn btn-primary" onclick="modemAction('imei')">查询 IMEI</button></div>
          <div class="result-box" id="modemQueryResult"></div>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-plane"></use></svg> 飞行模式</div>
        <div class="card-body">
          <div class="btn-row"><button class="btn btn-danger" id="flightBtn" onclick="toggleFlightMode()">切换飞行模式</button><button class="btn btn-secondary" onclick="queryFlightMode()">查询状态</button></div>
          <p class="form-hint">飞行模式开启后模组射频关闭，无法收发短信</p>
          <div class="result-box" id="flightResult"></div>
        </div>
      </div>
    </div>

    <!-- ===== AT Terminal ===== -->
    <div class="panel" id="panel-atterm">
      <h1 class="page-title">AT 指令终端</h1>
      <p class="page-subtitle">直接向模组发送 AT 指令并接收响应</p>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-terminal"></use></svg> 终端</div>
        <div class="card-body">
          <div id="atLog">就绪 — 输入 AT 指令开始调试</div>
          <div class="at-bar"><input class="form-input" type="text" id="atCmd" aria-label="AT 指令" placeholder="AT+CSQ"><button class="btn btn-primary btn-sm" onclick="sendAT()" id="atBtn">发送</button></div>
          <div class="btn-row" style="margin-top:8px;"><button class="btn btn-secondary btn-sm" onclick="clearATLog()">清空日志</button></div>
          <p class="form-hint">直接向模组串口发送指令并接收响应，请谨慎操作</p>
        </div>
      </div>
    </div>

    <!-- ===== System Log ===== -->
    <div class="panel" id="panel-log">
      <h1 class="page-title">系统日志</h1>
      <p class="page-subtitle">实时查看设备串口日志输出 <span id="logStatus" class="status-live">● 待命</span></p>
      <div class="card">
        <div class="card-header"><svg class="ui-icon" aria-hidden="true"><use href="#i-list"></use></svg> 日志输出</div>
        <div class="card-body">
          <div id="logView">加载中...</div>
          <div class="btn-row" style="margin-top:8px;">
            <button class="btn btn-secondary btn-sm" onclick="clearLogUI()">清空显示</button>
            <button class="btn btn-secondary btn-sm" onclick="refreshLog()">手动刷新</button>
            <label style="margin-left:8px;font-size:13px;cursor:pointer;"><input type="checkbox" id="logAuto" checked onchange="toggleLogAuto()"> 自动刷新</label>
          </div>
          <p class="form-hint">显示设备运行时输出的日志信息，每2秒自动刷新。日志最多保留最近120条。</p>
        </div>
      </div>
    </div>

  </main>

  <div class="modal-backdrop" id="apPasswordModal" hidden>
    <div class="modal-card" role="dialog" aria-modal="true" aria-labelledby="apPasswordTitle">
      <h2 id="apPasswordTitle">首次登录提示</h2>
      <p>设备默认热点密码为 <strong>%AP_DEFAULT_PASSWORD%</strong>。为了设备安全，请修改热点 SSID 的密码后再使用。</p>
      <div class="modal-actions">
        <button type="button" class="btn btn-secondary" onclick="closeApPasswordPrompt()">稍后</button>
        <button type="button" class="btn btn-primary" onclick="focusApPassword()">去修改</button>
      </div>
    </div>
  </div>

  <script>
    // ---- Panel switching ----
    function switchPanel(name) {
      var panel = document.getElementById('panel-' + name);
      if (!panel) return;
      document.querySelectorAll('.panel').forEach(function(p) { p.classList.remove('active'); });
      panel.classList.add('active');
      var items = document.querySelectorAll('.sidebar-nav a');
      var active = document.querySelector('.sidebar-nav a[data-panel="' + name + '"]');
      items.forEach(function(a) { a.classList.remove('active'); a.removeAttribute('aria-current'); });
      if (!active) return;
      active.classList.add('active');
      active.setAttribute('aria-current', 'page');
      window.scrollTo(0, 0);
      if (window.matchMedia('(max-width: 820px)').matches && active.scrollIntoView) {
        var calm = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
        active.scrollIntoView({ block: 'nearest', inline: 'center', behavior: calm ? 'auto' : 'smooth' });
      }
    }
    document.querySelectorAll('.sidebar-nav a').forEach(function(a) {
      var icon = a.querySelector('.ico');
      if (icon) icon.setAttribute('aria-hidden', 'true');
      a.setAttribute('role', 'button');
      a.setAttribute('tabindex', '0');
      a.addEventListener('click', function() { switchPanel(this.dataset.panel); });
      a.addEventListener('keydown', function(e) {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          switchPanel(this.dataset.panel);
        }
      });
    });

    // ---- Desktop liquid-glass specular ----
    (function() {
      var fine = window.matchMedia('(hover: hover) and (pointer: fine)');
      var calm = window.matchMedia('(prefers-reduced-motion: reduce)');
      var clear = window.matchMedia('(prefers-reduced-transparency: reduce)');
      if (!fine.matches || calm.matches || clear.matches) return;
      var frame = 0, card = null, px = 0, py = 0, lastMove = 0;
      function canRun() { return fine.matches && !calm.matches && !clear.matches; }
      document.addEventListener('pointermove', function(e) {
        if (!canRun()) return;
        var now = Date.now();
        if (now - lastMove < 32) return;
        lastMove = now;
        var next = e.target.closest ? e.target.closest('.card') : null;
        if (!next) return;
        card = next; px = e.clientX; py = e.clientY;
        if (frame) return;
        frame = requestAnimationFrame(function() {
          frame = 0;
          if (!card) return;
          var rect = card.getBoundingClientRect();
          if (!rect.width || !rect.height) return;
          var x = Math.max(0, Math.min(100, (px - rect.left) / rect.width * 100));
          var y = Math.max(0, Math.min(100, (py - rect.top) / rect.height * 100));
          card.style.setProperty('--glass-x', x.toFixed(1) + '%');
          card.style.setProperty('--glass-y', y.toFixed(1) + '%');
        });
      }, { passive: true });
    })();

    // ---- WiFi connection ----
    var wifiSavedAuth = Number('%WIFI_AUTH_MODE%');
    function wifiAuthFromScanCode(code) {
      if (code === 0) return 1;
      if (code === 6 || code === 7) return 4;
      if (code === 3) return 3;
      return 2;
    }
    function updateWifiPasswordState() {
      var auth = document.getElementById('wifiAuth');
      var pass = document.getElementById('wifiPass');
      if (!auth || !pass) return;
      var open = Number(auth.value) === 1;
      pass.disabled = open;
      if (open) pass.value = '';
    }
    function updateWifiAuthFromSelection() {
      var select = document.getElementById('wifiSsid');
      var auth = document.getElementById('wifiAuth');
      if (!select || !auth) return;
      var option = select.options[select.selectedIndex];
      if (option && option.dataset.auth !== undefined && option.dataset.auth !== '') {
        auth.value = String(wifiAuthFromScanCode(Number(option.dataset.auth)));
      }
      updateWifiPasswordState();
    }
    function refreshWifiStatus() {
      fetch('/wifi?action=status', {cache: 'no-store'}).then(function(r) { return r.json(); }).then(function(payload) {
        if (!payload.success) return;
        var status = document.getElementById('wifiStatusText');
        var ssid = document.getElementById('wifiCurrentSsid');
        var ip = document.getElementById('wifiCurrentIp');
        var rssi = document.getElementById('wifiCurrentRssi');
        if (status) status.textContent = payload.connected ? '已连接' : '未连接';
        if (ssid) ssid.textContent = payload.ssid || '--';
        if (ip) ip.textContent = payload.ip || '--';
        if (rssi) rssi.textContent = payload.connected ? (payload.rssi + ' dBm') : '--';
      }).catch(function() {});
    }
    function scanWifi() {
      var btn = document.getElementById('wifiScanBtn');
      var result = document.getElementById('wifiResult');
      var select = document.getElementById('wifiSsid');
      if (!btn || !select) return;
      var previous = select.value;
      btn.disabled = true;
      btn.textContent = '扫描中...';
      if (result) {
        result.className = 'result-box result-loading';
        result.textContent = '正在扫描附近 WiFi...';
      }
      fetch('/wifi?action=scan', {cache: 'no-store'}).then(function(r) { return r.json(); }).then(function(payload) {
        if (!payload.success) throw new Error(payload.message || '扫描失败');
        var networks = Array.isArray(payload.networks) ? payload.networks : [];
        select.textContent = '';
        if (networks.length === 0) {
          var empty = document.createElement('option');
          empty.value = '';
          empty.textContent = '未发现 WiFi';
          select.appendChild(empty);
        } else {
          networks.forEach(function(network) {
            var option = document.createElement('option');
            option.value = network.ssid;
            option.dataset.auth = String(network.auth);
            option.dataset.rssi = String(network.rssi);
            option.textContent = network.ssid + ' · ' + network.rssi + ' dBm · ' + (network.authText || '未知');
            select.appendChild(option);
          });
          var chosen = null;
          for (var i = 0; i < select.options.length; i++) {
            if (select.options[i].value === previous) { chosen = i; break; }
          }
          select.selectedIndex = chosen === null ? 0 : chosen;
        }
        updateWifiAuthFromSelection();
        if (result) {
          result.className = 'result-box result-success';
          result.textContent = networks.length > 0
            ? '发现 ' + networks.length + ' 个网络，选择后输入密码'
            : '没有发现可用的 WiFi';
        }
      }).catch(function(error) {
        if (result) {
          result.className = 'result-box result-error';
          result.textContent = '扫描失败：' + error.message;
        }
      }).finally(function() {
        btn.disabled = false;
        btn.textContent = '扫描';
      });
    }
    function saveWifi(connect) {
      var select = document.getElementById('wifiSsid');
      var auth = document.getElementById('wifiAuth');
      var pass = document.getElementById('wifiPass');
      var result = document.getElementById('wifiResult');
      if (!select || !auth || !pass) return;
      var ssid = select.value.trim();
      if (!ssid) {
        if (result) {
          result.className = 'result-box result-error';
          result.textContent = '请先选择 WiFi 网络';
        }
        return;
      }
      var data = new URLSearchParams();
      data.set('ssid', ssid);
      data.set('auth', auth.value);
      data.set('pass', pass.value);
      if (result) {
        result.className = 'result-box result-loading';
        result.textContent = connect ? '正在保存并连接...' : '正在保存...';
      }
      fetch('/wifi?action=' + (connect ? 'connect' : 'save'), {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'},
        body: data.toString()
      }).then(function(r) { return r.json(); }).then(function(payload) {
        if (!payload.success) throw new Error(payload.message || '保存失败');
        if (result) {
          result.className = 'result-box result-success';
          result.textContent = payload.message || 'WiFi 设置已保存';
        }
        if (connect) setTimeout(refreshWifiStatus, 8000);
      }).catch(function(error) {
        if (result) {
          result.className = 'result-box result-error';
          result.textContent = '操作失败：' + error.message;
        }
      });
    }

    // ---- AP hotspot ----
    var apPasswordChanged = Number('%AP_PASSWORD_CHANGED%');
    var apPasswordPromptNeeded = Number('%AP_PROMPT_NEEDED%');
    function updateApPasswordWarning() {
      var warning = document.getElementById('apPassWarning');
      if (warning) warning.style.display = apPasswordChanged ? 'none' : 'block';
    }
    function refreshApStatus() {
      fetch('/ap?action=status', {cache: 'no-store'}).then(function(r) { return r.json(); }).then(function(payload) {
        if (!payload.success) return;
        apPasswordChanged = payload.passwordChanged ? 1 : 0;
        var status = document.getElementById('apStatusText');
        var ip = document.getElementById('apIp');
        var clients = document.getElementById('apClients');
        if (status) status.textContent = payload.running ? '已启用' : '已关闭';
        if (ip) ip.textContent = payload.running ? (payload.ip || '--') : '--';
        if (clients) clients.textContent = String(payload.clients || 0);
        updateApPasswordWarning();
      }).catch(function() {});
    }
    function closeApPasswordPrompt() {
      var modal = document.getElementById('apPasswordModal');
      if (modal) modal.hidden = true;
    }
    function focusApPassword() {
      closeApPasswordPrompt();
      switchPanel('overview');
      var card = document.getElementById('apSsid');
      var pass = document.getElementById('apPass');
      if (card && card.scrollIntoView) card.scrollIntoView({behavior: 'smooth', block: 'center'});
      if (pass) setTimeout(function() { pass.focus(); }, 250);
    }
    function showApPasswordPrompt() {
      updateApPasswordWarning();
      if (apPasswordPromptNeeded && !apPasswordChanged) {
        var modal = document.getElementById('apPasswordModal');
        if (modal) modal.hidden = false;
      }
    }
    function saveApConfig() {
      var enabled = document.getElementById('apEnabled');
      var ssid = document.getElementById('apSsid');
      var pass = document.getElementById('apPass');
      var result = document.getElementById('apResult');
      if (!enabled || !ssid || !pass) return;
      var ssidValue = ssid.value.trim();
      if (!ssidValue) {
        if (result) {
          result.className = 'result-box result-error';
          result.textContent = '请输入热点 SSID';
        }
        return;
      }
      var data = new URLSearchParams();
      if (enabled.checked) data.set('enabled', 'on');
      data.set('ssid', ssidValue);
      data.set('pass', pass.value);
      if (result) {
        result.className = 'result-box result-loading';
        result.textContent = '正在保存并应用热点配置...';
      }
      fetch('/ap?action=save', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'},
        body: data.toString()
      }).then(function(r) { return r.json(); }).then(function(payload) {
        if (!payload.success) throw new Error(payload.message || '保存失败');
        apPasswordChanged = payload.passwordChanged ? 1 : 0;
        pass.value = '';
        updateApPasswordWarning();
        if (result) {
          result.className = 'result-box result-success';
          result.textContent = payload.message || '热点配置已保存';
        }
        setTimeout(refreshApStatus, 800);
      }).catch(function(error) {
        if (result) {
          result.className = 'result-box result-error';
          result.textContent = '操作失败：' + error.message;
        }
      });
    }

    // ---- Push network ----
    var pushNetMode = Number('%PUSH_NET_MODE%');
    var pushNetBusy = false;
    function pushNetName(mode) {
      if (mode === 1) return '4G 数据';
      if (mode === 2) return 'WiFi 优先 · 数据兜底';
      return 'WiFi';
    }
    function pushNetDescription(mode) {
      if (mode === 1) return '仅通过 4G 数据推送，会消耗流量；WiFi 是否连接不影响推送。';
      if (mode === 2) return '优先使用 WiFi；WiFi 未连接或连接失败时自动使用 4G 数据，WiFi 返回的业务错误不会重复发送。';
      return '仅通过 WiFi 推送；WiFi 未连接时不发送，也不会使用 4G 流量。';
    }
    function applyPushNetMode(mode) {
      var safeMode = (mode === 1 || mode === 2) ? mode : 0;
      document.querySelectorAll('.net-option').forEach(function(btn) {
        var active = Number(btn.dataset.pushNet) === safeMode;
        btn.classList.toggle('active', active);
        btn.setAttribute('aria-pressed', active ? 'true' : 'false');
      });
      var current = document.getElementById('pushNetCurrent');
      var hint = document.getElementById('pushNetHint');
      if (current) current.textContent = pushNetName(safeMode);
      if (hint) hint.textContent = pushNetDescription(safeMode);
    }
    function setPushNetMode(mode) {
      if (pushNetBusy || mode === pushNetMode) return;
      var result = document.getElementById('pushNetResult');
      var buttons = document.querySelectorAll('.net-option');
      pushNetBusy = true;
      buttons.forEach(function(btn) { btn.disabled = true; });
      if (result) {
        result.className = 'result-box result-loading';
        result.textContent = '正在保存网络设置...';
      }
      var data = new URLSearchParams();
      data.set('mode', mode);
      fetch('/pushnet', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'},
        body: data.toString()
      }).then(function(response) { return response.json(); }).then(function(payload) {
        if (!payload.success) throw new Error(payload.message || '保存失败');
        pushNetMode = Number(payload.mode);
        applyPushNetMode(pushNetMode);
        if (result) {
          result.className = 'result-box result-success';
          result.textContent = payload.message || '推送网络已切换';
        }
      }).catch(function(error) {
        if (result) {
          result.className = 'result-box result-error';
          result.textContent = '切换失败：' + error.message;
        }
      }).finally(function() {
        pushNetBusy = false;
        buttons.forEach(function(btn) { btn.disabled = false; });
      });
    }

    // ---- Push Channel JS ----
    function toggleChannel(idx) {
      var ch = document.getElementById('channel' + idx);
      var cb = document.getElementById('push' + idx + 'en');
      if (cb.checked) ch.classList.add('enabled'); else ch.classList.remove('enabled');
    }
    function testPush(idx) {
      var btn = document.getElementById('testBtn' + idx);
      var box = document.getElementById('testResult' + idx);
      if (!btn || !box) return;
      var data = new URLSearchParams();
      data.set('index', idx);
      ['type', 'name', 'url', 'key1', 'key2', 'body'].forEach(function(field) {
        var input = document.querySelector('[name="push' + idx + field + '"]');
        data.set(field, input ? input.value : '');
      });
      btn.disabled = true;
      btn.textContent = '测试中...';
      box.className = 'result-box push-test-result result-loading';
      box.textContent = '正在发送测试推送；4G 模式可能需要约 30 秒...';
      fetch('/testpush', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'},
        body: data.toString()
      }).then(function(rr) { return rr.json(); }).then(function(d) {
        box.className = 'result-box push-test-result ' + (d.success ? 'result-success' : 'result-error');
        box.textContent = d.message || (d.success ? '测试推送已发送' : '测试推送失败');
      }).catch(function(e) {
        box.className = 'result-box push-test-result result-error';
        box.textContent = '请求失败: ' + e;
      }).finally(function() {
        btn.disabled = false;
        btn.textContent = '测试';
      });
    }
    function updateTypeHint(idx) {
      var sel = document.getElementById('push' + idx + 'type');
      var hint = document.getElementById('hint' + idx);
      var extra = document.getElementById('extra' + idx);
      var custom = document.getElementById('custom' + idx);
      var type = parseInt(sel.value);
      // 按平台预填官方默认接口地址
      // 替换规则：字段为空、或当前值恰好是某平台默认地址时才更新；用户手输的自定义地址不动
      var urlInput = document.getElementById('url' + idx);
      if (urlInput) {
        var defUrls = {4:'https://oapi.dingtalk.com/robot/send',5:'http://www.pushplus.plus/send',8:'https://open.feishu.cn/open-apis/bot/v2/hook/',10:'https://api.telegram.org'};
        var urlPhs = {1:'http://your-server.com/api',2:'https://api.day.app/你的Key（或自建服务器地址）',3:'http://your-server.com/api',6:'留空将自动用 SendKey 拼接官方接口',7:'http://your-server.com/api',9:'https://你的Gotify服务器地址',11:'https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=...'};
        var cur = urlInput.value;
        var isDefaultVal = false;
        for (var k in defUrls) { if (defUrls[k] === cur) { isDefaultVal = true; break; } }
        if ((!cur || isDefaultVal) && defUrls[type]) urlInput.value = defUrls[type];
        urlInput.placeholder = urlPhs[type] || 'http://your-server.com/api 或 webhook地址';
      }
      extra.style.display = 'none'; custom.style.display = 'none';
      document.getElementById('key1label' + idx).innerText = '参数 1';
      document.getElementById('key2label' + idx).innerText = '参数 2';
      document.getElementById('key1' + idx).placeholder = '';
      document.getElementById('key2' + idx).placeholder = '';
      var kg = document.getElementById('key2group' + idx);
      if (kg) kg.style.display = 'none';
      if (type == 1) hint.innerHTML = 'POST JSON<br>{"sender":"+8613800138000","message":"...","timestamp":"2026-01-01 12:00:00"}';
      else if (type == 2) hint.innerHTML = 'Bark (iOS)<br>POST {"title":"发送者","body":"短信内容"}';
      else if (type == 3) hint.innerHTML = 'GET 请求<br>URL?sender=xxx&message=xxx&timestamp=xxx';
      else if (type == 4) { hint.innerHTML = '钉钉机器人<br>填写 Webhook 地址，加签需填 Secret'; extra.style.display='block'; document.getElementById('key1label'+idx).innerText='Secret（加签密钥，可选）'; document.getElementById('key1'+idx).placeholder='SEC...'; }
      else if (type == 5) { hint.innerHTML = 'PushPlus<br>填写 Token，URL 留空使用默认'; extra.style.display='block'; document.getElementById('key1label'+idx).innerText='Token'; document.getElementById('key1'+idx).placeholder='pushplus token'; if(kg)kg.style.display='block'; document.getElementById('key2label'+idx).innerText='发送渠道'; document.getElementById('key2'+idx).placeholder='wechat / extension / app'; }
      else if (type == 6) { hint.innerHTML = 'Server酱<br>填写 SendKey，URL 留空使用默认'; extra.style.display='block'; document.getElementById('key1label'+idx).innerText='SendKey'; document.getElementById('key1'+idx).placeholder='SCT...'; }
      else if (type == 7) { hint.innerHTML = '自定义模板<br>使用 {sender} {message} {timestamp} 占位符'; custom.style.display='block'; }
      else if (type == 8) { hint.innerHTML = '飞书机器人<br>已预填官方地址，需在其末尾拼接你的 Hook Token；签名验证另填 Secret'; extra.style.display='block'; document.getElementById('key1label'+idx).innerText='Secret（签名密钥，可选）'; document.getElementById('key1'+idx).placeholder='飞书签名密钥'; }
      else if (type == 9) { hint.innerHTML = 'Gotify<br>填写服务器地址 + 应用 Token'; extra.style.display='block'; document.getElementById('key1label'+idx).innerText='Token（应用 Token）'; document.getElementById('key1'+idx).placeholder='A...'; }
      else if (type == 10) { hint.innerHTML = 'Telegram Bot<br>Chat ID（参数1）+ Bot Token（参数2）'; extra.style.display='block'; document.getElementById('key1label'+idx).innerText='Chat ID'; document.getElementById('key1'+idx).placeholder='123456789'; if(kg)kg.style.display='block'; document.getElementById('key2label'+idx).innerText='Bot Token'; document.getElementById('key2'+idx).placeholder='12345678:ABC...'; }
      else if (type == 11) hint.innerHTML = '企业微信机器人<br>text 消息；填写群机器人 Webhook 地址（含 key）';
    }
    function toggleOperatorFields() {
      var mode = document.getElementById('operatorMode');
      var fields = document.getElementById('manualOperatorFields');
      if (mode && fields) fields.style.display = mode.value === '1' ? 'block' : 'none';
    }
    document.addEventListener('DOMContentLoaded', function() {
      var wifiAuthSelect = document.getElementById('wifiAuth');
      if (wifiAuthSelect) {
        wifiAuthSelect.value = String(wifiSavedAuth);
        wifiAuthSelect.addEventListener('change', updateWifiPasswordState);
      }
      var wifiSsidSelect = document.getElementById('wifiSsid');
      if (wifiSsidSelect) wifiSsidSelect.addEventListener('change', updateWifiAuthFromSelection);
      updateWifiPasswordState();
      refreshWifiStatus();
      updateApPasswordWarning();
      showApPasswordPrompt();
      toggleOperatorFields();
      applyPushNetMode(pushNetMode);
      for (var i = 0; i < 5; i++) { toggleChannel(i); updateTypeHint(i); }
    });

    // ---- Send SMS ----
    function updateCount(el) { document.getElementById('charCount').textContent = el.value.length; }

    // ---- Query ----
    function queryInfo(type) {
      var r = document.getElementById('queryResult');
      r.className = 'result-box result-loading'; r.textContent = '查询中...';
      fetch('/query?type=' + type).then(function(rr){return rr.json()}).then(function(d){
        if(d.success){r.className='result-box result-info';r.innerHTML=d.message;}
        else{r.className='result-box result-error';r.innerHTML='查询失败: '+d.message;}
      }).catch(function(e){r.className='result-box result-error';r.textContent='请求失败: '+e;});
    }

    // ---- Ping ----
    function confirmPing(){if(confirm('确定要执行 Ping 吗？将消耗少量流量。'))doPing();}
    function doPing(){
      var b=document.getElementById('pingBtn'),r=document.getElementById('pingResult');
      b.disabled=true;b.textContent='Pinging...';
      r.className='result-box result-loading';r.textContent='正在 Ping 8.8.8.8（最长 30 秒）...';
      fetch('/ping',{method:'POST'}).then(function(rr){return rr.json()}).then(function(d){
        b.disabled=false;b.textContent='Ping 8.8.8.8';
        if(d.success){r.className='result-box result-success';r.innerHTML='Ping 成功 — '+d.message;}
        else{r.className='result-box result-error';r.innerHTML='Ping 失败 — '+d.message;}
      }).catch(function(e){b.disabled=false;b.textContent='Ping 8.8.8.8';r.className='result-box result-error';r.textContent='请求失败: '+e;});
    }

    // ---- System Control ----
    var sysReloadTimer = null;
    function scheduleSysReload(){
      if (sysReloadTimer) return;
      var left = 30, r = document.getElementById('sysRestartResult');
      r.className = 'result-box result-success';
      r.textContent = '系统重启中，' + left + ' 秒后自动刷新页面...';
      sysReloadTimer = setInterval(function(){
        left--;
        if(left <= 0){ clearInterval(sysReloadTimer); location.reload(); }
        else r.textContent = '系统重启中，' + left + ' 秒后自动刷新页面...';
      }, 1000);
    }
    function systemRestart(){
      if(!confirm('确定要重启整个系统吗？重启期间将无法接收短信和访问网页。'))return;
      var r=document.getElementById('sysRestartResult');
      r.className='result-box result-loading';r.textContent='正在发送重启请求...';
      fetch('/system?action=restart').then(function(rr){return rr.json()}).then(function(d){
        scheduleSysReload();
      }).catch(function(e){
        // 重启导致连接中断属正常现象
        scheduleSysReload();
      });
    }

    // ---- WiFi Control ----
    function wifiRestart(){
      if(!confirm('确定要重启WiFi吗？网页将暂时不可用。'))return;
      var r=document.getElementById('wifiResult');
      r.className='result-box result-loading';r.textContent='WiFi 重启中（约5秒）...';
      fetch('/wifi?action=restart').then(function(rr){return rr.json()}).then(function(d){
        r.className=d.success?'result-box result-success':'result-box result-error';
        r.textContent=d.message;
      }).catch(function(e){r.className='result-box result-error';r.textContent='请求失败: '+e;});
    }

    // ---- Flight Mode ----
    function queryFlightMode(){
      var r=document.getElementById('flightResult');
      r.className='result-box result-loading';r.textContent='查询中...';
      fetch('/flight?action=query').then(function(rr){return rr.json()}).then(function(d){
        if(d.success){r.className='result-box result-info';r.innerHTML=d.message;}
        else{r.className='result-box result-error';r.innerHTML='查询失败: '+d.message;}
      }).catch(function(e){r.className='result-box result-error';r.textContent='请求失败: '+e;});
    }
    function toggleFlightMode(){
      if(!confirm('确定要切换飞行模式吗？'))return;
      var b=document.getElementById('flightBtn'),r=document.getElementById('flightResult');
      b.disabled=true;r.className='result-box result-loading';r.textContent='切换中...';
      fetch('/flight?action=toggle').then(function(rr){return rr.json()}).then(function(d){
        b.disabled=false;
        if(d.success){r.className='result-box result-success';r.innerHTML=d.message;}
        else{r.className='result-box result-error';r.innerHTML='切换失败: '+d.message;}
      }).catch(function(e){b.disabled=false;r.className='result-box result-error';r.textContent='请求失败: '+e;});
    }

    // ---- Modem Control ----
    function modemAction(action){
      var names={'restart':'软重启','hardreset':'硬重启','signal':'信号查询','operator':'运营商查询','imei':'IMEI查询'};
      var name=names[action]||action;
      var resultEl=null;
      if(action==='restart'||action==='hardreset') resultEl=document.getElementById('modemRstResult');
      else resultEl=document.getElementById('modemQueryResult');
      if(action==='hardreset'){
        if(!confirm('硬重启将断电重启模组，确定继续？'))return;
        resultEl.className='result-box result-loading';resultEl.textContent='硬重启中（约10秒）...';
        fetch('/modem?action=hardreset').then(function(rr){return rr.json()}).then(function(d){
          resultEl.className='result-box result-success';resultEl.textContent=d.message+' — 稍后请手动查询信号确认恢复';
        }).catch(function(e){resultEl.className='result-box result-error';resultEl.textContent='请求失败: '+e;});
        return;
      }
      resultEl.className='result-box result-loading';resultEl.textContent=name+'中...';
      fetch('/modem?action='+action).then(function(rr){return rr.json()}).then(function(d){
        if(d.success){resultEl.className='result-box result-success';resultEl.innerHTML=name+'成功: '+d.message;}
        else{resultEl.className='result-box result-error';resultEl.innerHTML=name+'失败: '+d.message;}
      }).catch(function(e){resultEl.className='result-box result-error';resultEl.textContent='请求失败: '+e;});
    }

    // ---- AT Terminal ----
    function addLog(msg,type){
      type=type||'resp';var log=document.getElementById('atLog'),div=document.createElement('div'),b=document.createElement('b');
      if(type==='user'){b.style.color='#fff';b.textContent='> ';}
      else if(type==='error'){b.style.color='#f44336';b.textContent='! ';}
      else{b.style.color='#50e3c2';b.textContent='';}
      div.appendChild(b);div.appendChild(document.createTextNode(msg));
      log.appendChild(div);log.scrollTop=log.scrollHeight;
    }
    function sendAT(){
      var inp=document.getElementById('atCmd'),cmd=inp.value.trim();if(!cmd)return;
      var btn=document.getElementById('atBtn');btn.disabled=true;btn.textContent='...';
      addLog(cmd,'user');inp.value='';
      fetch('/at?cmd='+encodeURIComponent(cmd)).then(function(rr){return rr.json()}).then(function(d){
        addLog(d.message,d.success?'resp':'error');
      }).catch(function(e){addLog('网络错误: '+e,'error')}).finally(function(){btn.disabled=false;btn.textContent='发送';});
    }
    function clearATLog(){var l=document.getElementById('atLog');l.innerHTML='';addLog('日志已清空','resp');}
    document.getElementById('atCmd').addEventListener('keydown',function(e){if(e.key==='Enter')sendAT();});

    // ---- Forward History ----
    var historyLoading = false;
    function historyStatus(d) {
      var status = Number(d.status);
      var classes = ['ok', 'partial', 'error', 'muted', 'muted', 'muted', 'error', 'error'];
      if (!isFinite(status) || status < 0 || status >= classes.length) status = 0;
      return {text: d.statusText || '已转发', cls: classes[status]};
    }
    function renderHistory(items) {
      var list = document.getElementById('historyList');
      if (!list) return;
      list.textContent = '';
      if (!Array.isArray(items) || items.length === 0) {
        var empty = document.createElement('div');
        empty.className = 'history-empty';
        empty.textContent = '暂无转发记录';
        list.appendChild(empty);
        return;
      }
      items.forEach(function(d) {
        var item = document.createElement('article');
        item.className = 'history-item';

        var top = document.createElement('div');
        top.className = 'history-top';
        var sender = document.createElement('div');
        sender.className = 'history-sender';
        sender.textContent = d.sender || '未知号码';
        var time = document.createElement('div');
        time.className = 'history-time';
        time.textContent = d.timestamp || '--';
        top.appendChild(sender);
        top.appendChild(time);

        var message = document.createElement('div');
        message.className = 'history-message';
        message.textContent = (d.message || '') + (d.truncated ? '\n…' : '');

        var reason = null;
        if (d.reason) {
          reason = document.createElement('div');
          reason.className = 'history-reason';
          reason.textContent = '原因：' + d.reason;
        }

        var channels = Array.isArray(d.channels) ? d.channels : [];
        var channelBox = null;
        if (channels.length > 0) {
          channelBox = document.createElement('div');
          channelBox.className = 'history-channels';
          channels.forEach(function(c) {
            var chip = document.createElement('span');
            chip.className = 'history-channel ' + (c.success ? 'ok' : 'error');
            chip.textContent = (c.name || '通道') + ' · ' + (c.success ? '成功' : '失败');
            channelBox.appendChild(chip);
          });
        }

        var footer = document.createElement('div');
        footer.className = 'history-footer';
        var state = historyStatus(d);
        var status = document.createElement('span');
        status.className = 'history-status ' + state.cls;
        status.textContent = state.text;
        var del = document.createElement('button');
        del.type = 'button';
        del.className = 'btn btn-danger btn-sm history-delete';
        del.textContent = '删除';
        del.addEventListener('click', function() { deleteHistory(d.id); });
        footer.appendChild(status);
        footer.appendChild(del);

        item.appendChild(top);
        item.appendChild(message);
        if (reason) item.appendChild(reason);
        if (channelBox) item.appendChild(channelBox);
        item.appendChild(footer);
        list.appendChild(item);
      });
    }
    function loadHistory() {
      if (historyLoading) return;
      var list = document.getElementById('historyList');
      if (!list) return;
      historyLoading = true;
      list.textContent = '加载中...';
      fetch('/history', {cache: 'no-store'}).then(function(r) { return r.json(); }).then(function(items) {
        renderHistory(items);
      }).catch(function(e) {
        list.textContent = '加载失败: ' + e;
      }).finally(function() {
        historyLoading = false;
      });
    }
    function deleteHistory(id) {
      if (!confirm('确定删除这条转发记录吗？')) return;
      var data = new URLSearchParams();
      data.set('action', 'delete');
      data.set('id', id);
      fetch('/history', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'},
        body: data.toString()
      }).then(function(r) { return r.json(); }).then(function(d) {
        if (!d.success) alert(d.message || '删除失败');
        loadHistory();
      }).catch(function(e) {
        alert('删除失败: ' + e);
      });
    }

    // ---- Log Viewer ----
    var logTimer = null;
    var logLoading = false;
    function setLogStatus() {
      var status = document.getElementById('logStatus');
      if (!status) return;
      var active = document.getElementById('panel-log').classList.contains('active');
      var auto = document.getElementById('logAuto').checked;
      if (active && auto && logTimer) status.textContent = '● 自动刷新中';
      else if (!auto) status.textContent = '● 已暂停';
      else status.textContent = '● 待命';
    }
    function startLogPoll() {
      var auto = document.getElementById('logAuto');
      var active = document.getElementById('panel-log').classList.contains('active');
      if (logTimer || !auto.checked || !active) return;
      logTimer = setInterval(refreshLog, 2000);
      setLogStatus();
    }
    function stopLogPoll() {
      if (logTimer) {
        clearInterval(logTimer);
        logTimer = null;
      }
      setLogStatus();
    }
    function toggleLogAuto() {
      if (document.getElementById('logAuto').checked) {
        refreshLog();
        startLogPoll();
      } else {
        stopLogPoll();
      }
      setLogStatus();
    }
    function clearLogUI() {
      document.getElementById('logView').textContent = '';
      var auto = document.getElementById('logAuto');
      if (auto.checked) {
        auto.checked = false;
        stopLogPoll();
      }
    }
    function refreshLog() {
      if (logLoading) return;
      var el = document.getElementById('logView');
      if (!el) return;
      var followTail = el.scrollHeight - el.scrollTop - el.clientHeight < 24;
      logLoading = true;
      fetch('/log').then(function(r) { return r.json(); }).then(function(lines) {
        if (!Array.isArray(lines)) return;
        el.textContent = lines.join('\n');
        if (followTail) el.scrollTop = el.scrollHeight;
      }).catch(function() {
        if (el.textContent === '加载中...') el.textContent = '无法获取日志';
      }).finally(function() { logLoading = false; });
    }
    var _origSwitchPanel = switchPanel;
    switchPanel = function(name) {
      _origSwitchPanel(name);
      if (name === 'log') {
        if (document.getElementById('logAuto').checked) {
          refreshLog();
          startLogPoll();
        } else stopLogPoll();
      } else {
        stopLogPoll();
      }
      if (name === 'history') loadHistory();
    };
    document.addEventListener('DOMContentLoaded', function() {
      var fieldCounter = 0;
      document.querySelectorAll('.form-group').forEach(function(group) {
        var label = group.querySelector('label');
        var control = group.querySelector('input, select, textarea');
        if (!label || !control) return;
        if (!control.id) control.id = 'field-' + (++fieldCounter);
        label.htmlFor = control.id;
      });
      document.querySelectorAll('.result-box').forEach(function(box) {
        box.setAttribute('role', 'status');
        box.setAttribute('aria-live', 'polite');
      });
      var atLog = document.getElementById('atLog');
      if (atLog) atLog.setAttribute('aria-live', 'polite');
      setLogStatus();
    });
  </script>
</body>
</html>
)rawliteral";
