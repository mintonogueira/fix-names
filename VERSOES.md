# Versões publicadas

Este repositório preserva as versões oficiais do `fix-names` em sequência
cronológica. Cada referência abaixo aponta para uma árvore completa do projeto,
permitindo consultar, compilar ou comparar qualquer entrega sem misturar seus
arquivos com os de outra versão.

| Versão | Referência | SHA-256 do pacote-fonte original | Observação |
| --- | --- | --- | --- |
| 2.0.0 | [`release/v2.0.0`](https://github.com/mintonogueira/fix-names/tree/release/v2.0.0) | `8242af4f9a10a962925196c8f040740570509d5a861314ddc2033465a5a960ac` | base multinterface |
| 2.1.0 | [`release/v2.1.0`](https://github.com/mintonogueira/fix-names/tree/release/v2.1.0) | `da26777d6a7145e2b4b84f11e35425b6a33ef3fd4fb341e5f16a923eedfa6652` | arquitetura C++17 completa |
| 2.1.1 | [`release/v2.1.1`](https://github.com/mintonogueira/fix-names/tree/release/v2.1.1) | `12ec8b903483e93ce1c7bf57f7d4a0add30e28bdf39e0adefab6514d6eb6f48d` | integridade da entrega |
| 2.1.2 | [`release/v2.1.2`](https://github.com/mintonogueira/fix-names/tree/release/v2.1.2) | `9be7b2bbb3db545bad1d71ed6dde955e3cbafda1ca80a990c8f012764e95147a` | progresso e bloqueio das GUIs |
| 2.1.3 | [`release/v2.1.3`](https://github.com/mintonogueira/fix-names/tree/release/v2.1.3) | `e8936f81e946c01aca4bdb681bca40bad23744cc41e3bbdd6983c82c11828439` | destinos de pacote separados |
| 2.1.4 | [`release/v2.1.4`](https://github.com/mintonogueira/fix-names/tree/release/v2.1.4) | `7137102bf779094a45db41f022adfa4b811bd1094e265fd21a1ff3dacab44402` | empacotadores autossuficientes |
| 2.1.5 | [`release/v2.1.5`](https://github.com/mintonogueira/fix-names/tree/release/v2.1.5) | `66d9ce16ed337901099d527bfe6444b244539662ad1e74fd8845256cb36fea58` | correção de `fixnames::tr()` na GUI Qt |
| 2.1.6 | [`release/v2.1.6`](https://github.com/mintonogueira/fix-names/tree/release/v2.1.6) | `5cfd8c1bf03f15db2656a9562d841f39427f71e857ac16e20ac5fa96a3e3330d` | edição completa documentada; Debian validado |
| 2.1.7 | [`release/v2.1.7`](https://github.com/mintonogueira/fix-names/tree/release/v2.1.7) | referência Git | corrige Qt/LTO no Arch com PIC/PIE consistente |

## Variante anterior da versão 2.1.6

Antes da edição documentada foi gerado o pacote
`fix-names-2.1.6-completo-atualizacao-corrigida.tar.gz`, com SHA-256
`be753e56817bba711cbc2320b2f703ca1e16b53a26f144fa2a199ca6a0510f14`.
A referência `release/v2.1.6` corresponde à edição documentada posterior, que
preserva o mesmo código funcional e acrescenta a documentação completa.

## Política do histórico

- Uma versão publicada nunca é reescrita.
- Correções futuras recebem um novo número de versão.
- Pacotes intermediários rejeitados ou incompletos não são versões oficiais.
- O Debian está confirmado desde a `2.1.6`.
- A falha de vinculação Qt/LTO observada no Arch na `2.1.6` foi corrigida na
  `2.1.7`, que também verifica PIE e `TEXTREL` automaticamente.
