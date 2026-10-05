<p align="center"><img src="docs/jangada.gif" alt="Jangada" width="720"></p>

<p align="center">
<a href="README.md">English</a> · <b>Português</b><br>
<a href="https://github.com/zednaked/jangada/actions/workflows/ci.yml"><img src="https://github.com/zednaked/jangada/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
<img src="https://img.shields.io/badge/licen%C3%A7a-GPL--3.0-ff14aa" alt="GPL-3.0">
<img src="https://img.shields.io/badge/M--VAVE-FM--1-ff14aa" alt="M-VAVE FM-1">
</p>

# Jangada 🛶

**Firmware alternativo para o M-VAVE FM-1**: dez motores de síntese (com FM de 6 operadores), um analógico com superwave,
matriz de modulação, drones que se sustentam sozinhos, ratchets e quatro trilhas, num synth de
bolso baratinho. Um fork do [Felucca](https://github.com/hugelton/Felucca) de Leo Kuroshita
(Hügelton Instruments). A felucca é o barco à vela do Nilo; a jangada é a nossa.

> **Alfa.** Use por sua conta e risco. A área de boot do FM-1 nunca é tocada, e dá para voltar ao
> firmware oficial a qualquer momento.

## Ouça

Gerados pelo próprio DSP do firmware (o mesmo código C, rodando num PC):

| Escuros / industriais | Drones | Superwave |
|---|---|---|
| [RUST BASS](docs/sounds/rust-bass.mp3) | [DRONE SAW](docs/sounds/drone-saw.mp3) | [SUPER SAW](docs/sounds/super-saw.mp3) |
| [HURT PAD](docs/sounds/hurt-pad.mp3) | [DRONE RING](docs/sounds/drone-ring.mp3) | [SUPER PAD](docs/sounds/super-pad.mp3) |
| [GRIND LEAD](docs/sounds/grind-lead.mp3) | [DRONE FM](docs/sounds/drone-fm.mp3) | [HP SHIMMER](docs/sounds/hp-shimmer.mp3) |
| [MACHINE](docs/sounds/machine.mp3) | [DRONE DUST](docs/sounds/drone-dust.mp3) | [quatro trilhas juntas](docs/sounds/four-tracks.mp3) |
| [METAL HIT](docs/sounds/metal-hit.mp3) | [DRONE VOX](docs/sounds/drone-vox.mp3) | |
| [BROKEN BELL](docs/sounds/broken-bell.mp3) · [STATIC](docs/sounds/static.mp3) | [DRONE ORGAN](docs/sounds/drone-organ.mp3) | |
| [QUIET KEYS](docs/sounds/quiet-keys.mp3) · [BROKEN KEY](docs/sounds/broken-key.mp3) | | |
| [GHOST KEYS](docs/sounds/ghost-keys.mp3) · [DIRTY ORGAN](docs/sounds/dirty-organ.mp3) | | |

## Instalar

**Linux**: conecte o FM-1 por um cabo USB de dados:

```
./instalar-linux.sh                    # a última versão publicada
./instalar-linux.sh --original         # volta ao firmware oficial da M-VAVE (V15)
./instalar-linux.sh --info             # o que o FM-1 está rodando
./instalar-linux.sh --console          # acesso ao console serial (regra udev, pede sudo)
```

O script prepara sozinho um ambiente Python (`mido` + `python-rtmidi`) em `~/.local/share/jangada/`.
**Mac / Windows**: o [instalador web](https://hugelton.github.io/Felucca/) do Felucca (Chrome ou Edge)
também instala o `.fwsc` das [releases](https://github.com/zednaked/jangada/releases).

## O que muda em relação ao Felucca

### Performance: segure um botão
Toque um botão de função e as páginas dele abrem, como sempre. **Segure** e ele vira uma
**camada**: as 16 teclas brancas e os 4 knobs mudam de função enquanto ele está apertado, e a tela
mostra as teclas como 16 tiles (4 × 4) e os knobs como dials. **HOME** tocado com a camada segurada
a **trava** aberta (as duas mãos livres); qualquer outro botão a solta. PLAY, REC e OCT continuam
valendo dentro dela. Ideia e boa parte do código do [SLOOP](https://github.com/isod89/sloop-fm1).

| Segure | Teclas | Knobs 1 · 2 · 3 · 4 |
|---|---|---|
| **FX**: punch | 16 efeitos no mix inteiro enquanto a tecla está apertada: loops 1/4 a 1/32, stutter, reverse, tape stop, half, LP / HP sweep, phone, crush, alias, gate, echo, wobble | FILT · DUST · DUCK |
| **GLO**: mix | 1–4 mute, 5–8 solo, a última tap tempo | nível das trilhas 1–4 |
| **SEQ**: passos | os 16 passos da página: vazio = cria com a última nota, cheio = apertar e soltar apaga. Pretas: F# G# A# C# = página; D#4 / F#4 desloca, G#4 / A#4 metade / dobro, C#5 / D#5 transpõe, **F#5 segurada apaga** o que o playhead passa | NOTE · DIV · SWG · LEN; com passos segurados: NOTE · RTCH · CHNC · FLAG |
| **SCL**: tom | qualquer tecla = o tom da música (todas as trilhas) | CHRD · SCL · QNT · TRN |
| **EDIT**: motor | 1–9 = o motor da trilha; a última = trilha 4 DRUM / SYNTH | PRST · VOICE · GLIDE · LVL |

Com **SEQ** segurado, **OCT− / OCT+** = undo / redo do padrão.

### Master
**GLO → MASTER** (e os knobs da camada FX): **DUST** (sampler velho e disco: bits, taxa, chiado
enquanto toca), **DUCK** (o bumbo abaixa os synths por uma colcheia), **FILT** (filtro de DJ:
esquerda passa-baixa, direita passa-alta).

### Acordes de uma tecla
**SCL → CHORD** (ou o knob 1 da camada SCL): OFF, TRIAD, 7TH, 9TH, SUS4, POWER. Ligado, as teclas
brancas andam pela escala a partir do C4 e cada uma toca o acorde da escala inteiro (gravado como
acorde no passo). A trilha passa a POLY sozinha.

### TRACKS
O **REC** numa página sem nada para gravar abre a tela de trilhas: BPM, compasso.beat, uma linha
por trilha com o som, o motor, os passos e o playhead, o nível e os selos REC / SOLO / MUTE.

### Som
- **ANALOG turbinado** (EDIT 3 / 4): **SUPR** superwave (até 6 cópias desafinadas do oscilador),
  **SDTN** abertura, **SUB** quadrada uma oitava abaixo, **DRFT** desafinação lenta por voz,
  **FTYP** filtro LP12 / LP24 / BP / HP. Com muitas vozes o superwave usa menos cópias, para caber
  na CPU (8 vozes de SUPER SAW: 55 % no FM-1).
- **Matriz de modulação**: botão LFO → páginas **MOD 1–4**. Cada slot: origem (LFO, ENV, VEL, KEY,
  RND) → destino (filtro, pitch, forma ou qualquer parâmetro do motor) × quantidade.
- **16 parâmetros por motor** (o Felucca tem 8).
- **20 presets novos**: texturas escuras e industriais, superwaves e seis drones.
- **Hi-hats e crash** do kit GM tocam a própria amostra (soavam como toms).
- **FM6**: FM de 6 operadores (o núcleo msfa do Dexed, portado pelo Felucca 1.0), 32 algoritmos,
  8 patches de fábrica (PTCH F1–F8) e macros nos knobs (ALG FB MLVL MRAT MEG VMOD DTUN). O DIGITAL de
  4 operadores continua.
- **Bateria sintetizada** (do SLOOP): **GLO → KIT**, ou o knob PRESETS na trilha de bateria: o kit GM
  sampleado ou 32 kits sintetizados (808, 909, TECHNO, INDUSTR, GLITCH, DUBSTEP, JUNGLE…).
- **Reverbs** (**FX → REVERB**, TYPE): ROOM (a de sempre), SPRING (mola, do Felucca 1.0) e PLATE
  (rede de atrasos estéreo, do SLOOP).

### MIDI
- **Pitch bend** (±2 semitons), **sustain** (CC64), **all notes off** (CC120 / 123), reset (CC121).
- **Mod wheel**, **aftertouch** e **expressão** (CC11) como origens da matriz (MODW, AT, EXPR).
- **GLO → GLOBAL CLK USB**: segue o clock do computador (tempo, start, stop).
- **GLO → SYSTEM SYNC OUT**: manda clock pelo USB (24 por batida, start, stop).

### Áudio por USB
O FM-1 aparece no computador como uma entrada de áudio estéreo (44,1 kHz, "Jangada"), sem driver:
grave o master direto na DAW (no Linux: `arecord -D hw:Jangada -f S16_LE -r 44100 -c 2 take.wav`).

### Memória e segurança
- **Autosave**: parado e sem mexer por alguns segundos, o projeto vai para a flash e volta ao ligar.
  **OCT+** segurado ao ligar começa vazio; **HOME → NEW PROJECT** zera tudo.
- **Atualização mais segura** (do SLOOP): o instalador recusa pacote danificado; o loader confere o
  CRC antes de liberar o firmware novo.
- **Resgate por USB**: **OCT−** segurado ao ligar (ou dois boots que falham) abre JANGADA USB RESCUE,
  onde só o instalador roda. A calibração do painel: **OCT− + OCT+** ao ligar.
- Menu (segure **HOME**): COLOR, SPEAKER (corta graves para o alto-falante), NEW PROJECT, ABOUT.

### Drones
Os presets DRONE usam o arpejador em **RPT** a cada **4 compassos** com **HOLD**: toque um acorde,
solte, e ele continua respirando sozinho, inclusive enquanto você toca outras trilhas.
- **Segure ARP** → DRONE OFF: solta os acordes presos (saem com o release do preset).
- **Segure ARP de novo** → SILENCE: as caudas param na hora.

Para sequenciar um drone: arp OFF, PATTERN com **DIV 4BAR**, um acorde por passo (cada passo dura
4 compassos).

### Arpejador e sequencer
- Modos **UDI** (sobe e desce repetindo as pontas) e **RPT** (o acorde inteiro a cada passo);
  divisões **1/2, 1/1, 2BAR, 4BAR** (também no sequencer).
- Página **STEP 2**: **RTCH** ratchet x1–x4 e **CHNC** chance 100/75/50/25 % por passo.

### Trilhas
- **Trilha 4: DRUM ou SYNTH.** Em **TRACKS**, escolha a trilha 4 com o ALGORITHM e gire o
  **knob 1 (TYPE)**: SYNTH a transforma numa quarta parte de synth (motor, preset, arp, sequencer,
  MIDI canal 4); DRUM volta ao kit GM.

### Tela e memória
- Paleta **CHOQUE** (rosa-choque) como padrão; as outras continuam no menu (segure HOME → COLOR).
- Projetos (**JNG1**) e presets de usuário (**UPB2**) guardam cada valor com uma **chave
  estável**: parâmetros podem ser acrescentados ou movidos sem perder o que foi salvo. Projetos e
  presets do Felucca são lidos e convertidos.

## Ferramentas

| | |
|---|---|
| `tools/fm1_console.py status` | CPU, áudio, USB, bateria |
| `tools/fm1_console.py check` | teste no aparelho: pico de CPU, atrasos de áudio, reinícios |
| `tools/fm1_console.py voices` | o que soa em cada trilha, e por quê |
| `tools/fm1_console.py preset E I [T]` | carrega o preset I do motor E na trilha T |
| `tools/fm1_console.py t4 synth\|drum` | tipo da trilha 4 |
| `tools/fm1_console.py droneoff` | como segurar ARP |
| `tools/fm1_console.py color CHOQUE` | paleta da tela |
| `tools/fm1_console.py g ID [VALOR]` | lê ou muda um parâmetro global (ex.: `g 28 90` = DUST) |
| `tools/fm1_console.py punch N\|off` | liga um efeito punch (0–15) ou desliga |

## Compilar e testar

Veja [BUILDING.md](BUILDING.md). No Linux x86-64 o toolchain da JieLi roda nativo, sem Docker:

```
tools/get_toolchain.sh        # o toolchain, em ~/.jieli
tools/get_sdk_files.sh        # só os 3 arquivos do SDK AC79 que o pacote usa
./build.sh                    # build/felucca.fwsc
sh tests/run_tests.sh         # todos os testes, no PC
```

- **Build reprodutível**: a data vem do último commit; dois builds dão os mesmos bytes.
- Os testes cobrem o som (renders de todos os presets com impressão digital), saúde (clipping, DC,
  notas presas), orçamento de CPU (contador de instruções no Linux e no Mac), formatos, arp, steps,
  a matriz, a trilha 4, o instalador e o editor web, cujas tabelas do simulador são geradas a
  partir do firmware (`tools/gen_editor_tables.py`).
- **CI** a cada push; uma tag `vX.Y[-sufixo]` publica o `.fwsc` numa release.

## Próximos passos

- Uma revisão da interface, para legibilidade (a fonte suavizada do Felucca 1.0).
- Editor de patches FM6 no editor web; backup de presets em `.syx`.

## Créditos e licença

A Jangada é GPL-3.0-only, como o Felucca. As camadas, o punch FX, o master e os acordes vêm do
[SLOOP](https://github.com/isod89/sloop-fm1) (GPL-3.0), outro fork do Felucca. O trabalho original é de **Leo Kuroshita (@kurogedelic),
Hügelton Instruments**: veja [README.felucca.md](README.felucca.md) e [LICENSING.md](LICENSING.md)
para os créditos completos (fontes, amostras, motores).

M-VAVE e FM-1 são marcas de seus donos. A Jangada não é afiliada nem endossada por eles, nem pelo
Felucca.
