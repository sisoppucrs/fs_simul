# Trabalho 2 Sistemas Operacionais PUCRS


1. Objetivos

    - Implementação de um sistema de arquivos simplificado, baseado em FAT.

    - Reforço no aprendizado sobre implementação de sistema de arquivos.

    - Aprendizado de manipulação de arquivos e estruturas em dispositivo de armazenamento secundário.


2. Descrição do sistema de arquivos

Este trabalho consiste na implementação de um simulador de sistema de arquivos. Um
programa desenvolvido pelo aluno, em C, deve manipular estruturas de dados em um arquivo
simulando o comportamento de um sistema de arquivos em disco. A implementação deve
seguir a especificação a seguir:

    - O sistema de arquivos utiliza método de alocação de blocos encadeado (conforme explicado em aula).

    - O sistema de arquivos deve considerar setores de 512 bytes.

    - O diretório raiz (root) fica no setor 0 e além de uma lista de arquivos e diretórios ele armazena um ponteiro para a lista de blocos livres.

    - Os setores são numerados de 0 a n.

    - O sistema de arquivos utiliza 4 bytes (32 bits) para numeração dos blocos.
Assim, são possíveis 2³² blocos de 512 bytes cada, totalizando 2 terabytes de
espaço total suportado pelo sistema de arquivos.

    - O tamanho do arquivo é armazenado em uma variável de 32bits. Portanto, um arquivo pode ter tamanho máximo de 4GB.

    - O sistema de arquivos utiliza mapeamento de blocos livres por encadeamento.

    - O diretório raiz guarda o ponteiro para o primeiro bloco livre.
