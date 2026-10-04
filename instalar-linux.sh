#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Instalador da Jangada para Linux: grava o firmware no M-VAVE FM-1 pelo cabo USB (USB-MIDI).
#
#   ./instalar-linux.sh                 instala a ultima versao publicada da Jangada
#   ./instalar-linux.sh ARQUIVO.fwsc    instala um pacote local (ex.: build/felucca.fwsc)
#   ./instalar-linux.sh --original      volta para o firmware oficial da M-VAVE (V15)
#   ./instalar-linux.sh --info          so mostra o que esta rodando no FM-1
#   ./instalar-linux.sh --console       libera o console serial do FM-1 (regra udev, pede sudo)
#   opcoes: --sim (nao pergunta)
#
# Precisa de: python3, um cabo USB de dados, o FM-1 ligado. Nada e gravado fora da area
# do aplicativo: o boot do FM-1 nunca e tocado (ver firmware/loader).
set -euo pipefail

REPO="zednaked/jangada"
AQUI="$(cd "$(dirname "$0")" && pwd)"
DADOS="${XDG_DATA_HOME:-$HOME/.local/share}/jangada"
VENV="$DADOS/venv"
ORIGINAL_URL="https://yms-file-store.oss-cn-hongkong.aliyuncs.com/software/firmware/FM-1.fwsc"
ORIGINAL_SHA="db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a"   # FM-1 V15 (2026-07-30)

rosa=$'\e[1;38;2;255;20;170m'; neg=$'\e[1m'; ver=$'\e[31m'; fim=$'\e[0m'
diga() { printf '%s==>%s %s\n' "$rosa" "$fim" "$*"; }
morra() { printf '%serro:%s %s\n' "$ver" "$fim" "$*" >&2; exit 1; }

modo=instalar; pacote=""; sim=0
for a in "$@"; do
    case "$a" in
        --original) modo=original ;;
        --info) modo=info ;;
        --console) modo=console ;;
        --sim|-y) sim=1 ;;
        -h|--help) sed -n '3,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*) morra "opcao desconhecida: $a (veja --help)" ;;
        *) pacote="$a" ;;
    esac
done

[ "$(uname -s)" = Linux ] || morra "este instalador e para Linux"

if [ "$modo" = console ]; then
    diga "instalando a regra udev do console (tools/70-jangada.rules); o sudo vai pedir sua senha"
    sudo install -m 644 "$AQUI/tools/70-jangada.rules" /etc/udev/rules.d/70-jangada.rules
    sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=tty --subsystem-match=usb
    diga "pronto: tools/fm1_console.py status  (ou: check)"
    exit 0
fi
command -v python3 >/dev/null || morra "precisa de python3"

# 1. ambiente Python com mido + python-rtmidi (uma vez so)
if ! "$VENV/bin/python" -c 'import mido, rtmidi' 2>/dev/null; then
    diga "preparando o ambiente Python em $VENV (so na primeira vez)"
    mkdir -p "$DADOS"
    if command -v uv >/dev/null; then
        uv venv -q "$VENV" && uv pip install -q -p "$VENV" mido python-rtmidi
    else
        python3 -m venv "$VENV" && "$VENV/bin/pip" install -q mido python-rtmidi
    fi
fi
PY="$VENV/bin/python"
INST="$AQUI/tools/fm1_install.py"
[ -f "$INST" ] || morra "nao achei $INST (rode de dentro do repositorio da Jangada)"

# 2. o FM-1 esta ai?
if ! grep -qiE 'fm-1|felucca|jangada' /proc/asound/cards 2>/dev/null; then
    morra "nao achei o FM-1. Ligue-o e conecte por um cabo USB de DADOS (sem hub, de preferencia)."
fi
if ! ls /dev/snd/midiC* >/dev/null 2>&1 || ! [ -r "$(ls /dev/snd/midiC* | head -1)" ]; then
    morra "sem permissao nos dispositivos MIDI (/dev/snd). Entre no grupo 'audio' ou use uma sessao local."
fi

if [ "$modo" = info ]; then
    exec "$PY" "$INST" --info
fi

# 3. qual pacote
extra=()
if [ "$modo" = original ]; then
    pacote="$DADOS/FM-1_V15_oficial.fwsc"
    if [ ! -f "$pacote" ]; then
        diga "baixando o firmware oficial da M-VAVE (V15)"
        curl -fsSL -o "$pacote.tmp" "$ORIGINAL_URL" || { rm -f "$pacote.tmp"; morra "nao consegui baixar o firmware oficial (internet?)"; }
        mv "$pacote.tmp" "$pacote"
    fi
    echo "$ORIGINAL_SHA  $pacote" | sha256sum -c --quiet - || { rm -f "$pacote"; morra "o arquivo oficial nao confere (sha256)"; }
    extra=(--force)          # o pacote oficial nao tem a marca do loader da Jangada/Felucca
elif [ -z "$pacote" ]; then
    command -v curl >/dev/null || morra "precisa de curl"
    diga "procurando a ultima versao da Jangada"
    achar() { grep -oE '"browser_download_url": *"[^"]+\.fwsc"' | head -1 | sed -E 's/.*"(http[^"]+)"/\1/'; }
    # a versao estavel mais nova; sem nenhuma, a pre-versao mais nova
    url="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" 2>/dev/null | achar)" || true
    [ -n "$url" ] || url="$(curl -fsSL "https://api.github.com/repos/$REPO/releases" | achar)" || true
    [ -n "$url" ] || morra "nenhuma versao publicada (ou sem internet). Passe um arquivo: ./instalar-linux.sh build/felucca.fwsc"
    pacote="$DADOS/$(basename "$url")"
    curl -fsSL -o "$pacote.tmp" "$url" || { rm -f "$pacote.tmp"; morra "nao consegui baixar $url"; }
    if sha="$(curl -fsSL "$url.sha256" 2>/dev/null)" && [ -n "$sha" ]; then
        [ "${sha%% *}" = "$(sha256sum "$pacote.tmp" | cut -d' ' -f1)" ] || { rm -f "$pacote.tmp"; morra "o arquivo baixado nao confere com o .sha256 da versao"; }
    fi
    mv "$pacote.tmp" "$pacote"
fi
[ -f "$pacote" ] || morra "arquivo nao encontrado: $pacote"

# 4. confirmar e gravar
"$PY" "$INST" --info || true
echo
printf '%sVou gravar:%s %s\n' "$neg" "$fim" "$pacote"
echo "  - mantenha o cabo conectado e nao deixe o computador dormir ate o fim;"
echo "  - o FM-1 reinicia sozinho; seus presets nao sao apagados."
if [ "$sim" = 0 ]; then
    read -r -p "Continuar? [s/N] " r
    [[ "$r" =~ ^[sSyY] ]] || morra "cancelado"
fi
"$PY" "$INST" "$pacote" --yes "${extra[@]}"
diga "pronto!"
