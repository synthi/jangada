# Jangada 🛶

Firmware multi-motor para o **M-VAVE FM-1**, um fork do
[Felucca](https://github.com/hugelton/Felucca) de Leo Kuroshita (Hügelton Instruments).
A felucca é o barco à vela do Nilo; a jangada é a nossa.

> **Alfa.** Use por sua conta e risco. O boot do FM-1 nunca é tocado e dá para voltar ao
> firmware oficial a qualquer momento (veja abaixo).

## Instalar no Linux

Conecte o FM-1 por um cabo USB de dados e rode:

```
./instalar-linux.sh                    # a última versão publicada
./instalar-linux.sh build/felucca.fwsc # um build seu
./instalar-linux.sh --original         # volta para o firmware oficial da M-VAVE (V15)
./instalar-linux.sh --info             # mostra o que está rodando
./instalar-linux.sh --console          # libera o console serial (regra udev, pede sudo)
```

O script prepara sozinho um ambiente Python (`mido` + `python-rtmidi`) em
`~/.local/share/jangada/`. No Chrome/Edge, o instalador web do Felucca também funciona.

## O que a Jangada traz

Tudo o que o Felucca tem (9 motores, 4 trilhas, sequencer de 64 passos, editor web), mais:

### Som
- **ANALOG turbinado** (EDIT 3 / 4): **SUPR** superwave (até 6 cópias desafinadas do
  oscilador), **SDTN** abertura, **SUB** quadrada uma oitava abaixo, **DRFT** desafinação lenta
  por voz, **FTYP** filtro LP12 / LP24 / BP / HP. Com muitas vozes o superwave usa menos cópias,
  para caber na CPU (8 vozes de SUPER SAW: 55 %).
- **Matriz de modulação**: botão LFO → páginas **MOD 1–4**. Cada slot: origem (LFO, ENV, VEL,
  KEY, RND) → destino (filtro, pitch, forma ou qualquer parâmetro do motor) × quantidade.
- **16 parâmetros por motor** (eram 8): as páginas EDIT 3 / 4 aparecem quando o motor os tem.
- **Pacote de sons**:
  - escuros / industriais: RUST BASS, HURT PAD, SUPER SAW, SUPER PAD, HP SHIMMER (ANALOG),
    GRIND LEAD, MACHINE (TRIO), METAL HIT (DIGITAL), BROKEN BEL (PHASE), STATIC (LOFI),
    QUIET KEYS, BROKEN KEY (SAMPLE), GHOST KEYS (GRAIN), DIRTY ORGN (WHEEL);
  - drones: DRONE SAW, DRONE RING, DRONE FM, DRONE DUST, DRONE VOX, DRONE ORGN.
- **Hi-hats e crash** do kit GM tocam a própria amostra (no Felucca soavam como toms).

### Drones
Os presets DRONE usam o arpejador em **RPT** a cada **4BAR** com **HOLD**: toque um acorde,
solte, e ele continua respirando sozinho, inclusive enquanto você toca outras trilhas.
- **Segure ARP** → DRONE OFF: solta os acordes presos (saem pelo release do preset).
- **Segure ARP de novo** → SILENCE: as caudas param na hora.

Para sequenciar um drone: arp OFF, PATTERN com **DIV 4BAR**, um acorde por passo (cada passo
dura 4 compassos).

### Arpejador e sequencer
- Arpejador: modos **UDI** (sobe e desce repetindo as pontas) e **RPT** (o acorde inteiro a cada
  passo); divisões **1/2, 1/1, 2BAR, 4BAR** (também no sequencer).
- Sequencer: página **STEP 2** com **RTCH** (ratchet x1–x4) e **CHNC** (chance 100/75/50/25 %)
  por passo.

### Trilhas
- **Trilha 4: DRUM ou SYNTH.** Em **TRACKS**, escolha a trilha 4 com o ALGORITHM e gire o
  **knob 1 (TYPE)**: SYNTH a transforma numa quarta parte de synth (motor, preset, arp,
  sequencer, MIDI canal 4); DRUM volta ao kit GM. Também em GLO → DRUMS → T4.

### Tela
- Paleta **CHOQUE** (rosa-choque) como padrão; as outras continuam no menu (segure HOME → COLOR).

### Salvar sem medo
- Projetos (formato **JNG1**) e presets de usuário (banco **UPB2**) guardam cada valor com uma
  **chave estável**: parâmetros podem ser acrescentados ou movidos sem perder o que foi salvo.
  Projetos e presets do Felucca são lidos e convertidos.

## Ferramentas

| | |
|---|---|
| `tools/fm1_console.py status` | estado do aparelho (CPU, áudio, USB, bateria) |
| `tools/fm1_console.py check` | teste no aparelho: CPU, atrasos de áudio, reinícios |
| `tools/fm1_console.py voices` | o que soa em cada trilha e por quê |
| `tools/fm1_console.py preset E I [T]` | carrega o preset I do motor E na trilha T |
| `tools/fm1_console.py t4 synth\|drum` | tipo da trilha 4 |
| `tools/fm1_console.py droneoff` | como segurar ARP |
| `tools/fm1_console.py color CHOQUE` | paleta da tela |

## Compilar e testar

Veja [BUILDING.md](BUILDING.md). No Linux x86-64 o toolchain da JieLi roda nativo, sem Docker:

```
tools/get_toolchain.sh        # o toolchain, em ~/.jieli
tools/get_sdk_files.sh        # só os 3 arquivos do SDK AC79 que o pacote usa
./build.sh                    # build/felucca.fwsc
sh tests/run_tests.sh         # todos os testes no PC
```

- O **build é reprodutível**: a data vem do último commit; dois builds dão o mesmo arquivo.
- Os testes cobrem som (renders com impressão digital), saúde (clipping, DC, notas presas),
  orçamento de CPU (contador de instruções no Linux e no Mac), formatos, arp, steps, matriz,
  trilha 4, instalador e editor web. As tabelas do simulador do editor são geradas a partir do
  firmware (`tools/gen_editor_tables.py`).
- **CI** no GitHub Actions a cada push; uma tag `vX.Y[-sufixo]` publica o `.fwsc` numa release.

## Para onde vamos

- **FM de 6 operadores** compatível com patches de DX7 (porte do msfa/Dexed).
- MIDI completo (pitch bend, sustain, clock), backup de presets em `.syx`.
- Uma revisão sutil da interface, para legibilidade.

Correções que servem a todos vão também como PR para o Felucca.

## Créditos e licença

Jangada é GPL-3.0-only, como o Felucca. Todo o trabalho original é de
**Leo Kuroshita (@kurogedelic), Hügelton Instruments**, veja [README.felucca.md](README.felucca.md)
e [LICENSING.md](LICENSING.md) para os créditos completos (fontes, samples, motores).

M-VAVE e FM-1 são marcas de seus donos. A Jangada não é afiliada nem endossada por eles,
nem pelo Felucca.
