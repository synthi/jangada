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
```

O script prepara sozinho um ambiente Python (`mido` + `python-rtmidi`) em
`~/.local/share/jangada/` e usa o instalador USB-MIDI do Felucca (`tools/fm1_install.py`).
No Chrome/Edge, o instalador web do Felucca também funciona.

## O que muda em relação ao Felucca

- Paleta **CHOQUE** (rosa-choque) como padrão; as outras continuam no menu (segure HOME → COLOR).
- Instalador para Linux, inclusive a volta ao firmware oficial.

### Para onde vamos

1. **Infraestrutura**: build Linux-first, CI com testes e build reprodutível, testes no aparelho
   pelo console serial.
2. **Núcleo**: parâmetros descritos por schema (sem o limite de 8 por motor), matriz de
   modulação, formato de preset com IDs.
3. **Caminhos novos**: FM de 6 operadores compatível com DX7, áudio pela USB, motores novos.

Correções que servem a todos vão também como PR para o Felucca.

## Compilar

Igual ao Felucca, veja [BUILDING.md](BUILDING.md). No Linux x86-64 o toolchain da JieLi roda
nativo, sem Docker; do SDK AC79 bastam os três arquivos listados lá.

## Créditos e licença

Jangada é GPL-3.0-only, como o Felucca. Todo o trabalho original é de
**Leo Kuroshita (@kurogedelic), Hügelton Instruments**, veja [README.felucca.md](README.felucca.md)
e [LICENSING.md](LICENSING.md) para os créditos completos (fontes, samples, motores).

M-VAVE e FM-1 são marcas de seus donos. A Jangada não é afiliada nem endossada por eles,
nem pelo Felucca.
