#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/**
Disciplina de BD II
Professor Guilherme Dal Bianco
Alunos Bruno Ribeiro e Rodrigo Costa
Simulador de tabela de BD distribuída em páginas (blocos)
*/

#define MFIELD 10
#define PAGE_SIZE 4096
#define STRING_MAX 256

struct theader {
    char name[15];
    char type;
    int  len;
};

struct record_id {
    int page;
    int slot;
};

union tint {
    char cint[sizeof(int)];
    int vint;
};

struct theader *readHeader() {
    FILE *f;
    struct theader *th=(struct theader*) malloc(sizeof(struct theader)*MFIELD);
    int i, bitmap_len;

    f=fopen("agenda.dat","r");

    if (f == NULL) {
        printf("File not found\n");
        exit(0);
    }

    fseek(f,1,SEEK_CUR);
    fread(&bitmap_len,sizeof(int),1,f);
    fseek(f,sizeof(int)+(sizeof(short)*bitmap_len),SEEK_CUR);

    // geramos o vetor com as informaćões dos campos
    for (i=0;i<MFIELD;i++) {
        fread(th[i].name,15,1,f);
        fread(&th[i].type,1,1,f);
        fread(&th[i].len,sizeof(int),1,f);
    }

    fclose(f);
    return th;
}

struct record_id create_page(int curr_page, int bitmap_len, int rec_len) {
    FILE *f;
    struct record_id rid;
    int i, slot_qty, qty_fill, slot=0, page=curr_page+1;
    char *fill, nextpage='N';
    short *bitmap;

    f=fopen("agenda.dat","r+b");

    if (f==NULL) {
        printf("File not found\n");
        exit(0);
    }

    fseek(f,page*PAGE_SIZE,SEEK_SET);

    // gravamos o header na primeira página, após ela teremos um char que indica se tem proxima pagina
    // em seguida um int que indica o tamanho do bitmap e na sequencia o bitmap e os registros
    qty_fill = PAGE_SIZE;
    // quantidade de slots
    slot_qty = bitmap_len;
    qty_fill -= slot_qty*sizeof(short);
    // flag proxima pagina
    fwrite(&nextpage,1,1,f);
    qty_fill -= 1;
    // numero de slots (tamanho do bitmap)
    fwrite(&slot_qty,sizeof(int),1,f);
    qty_fill -= sizeof(int);
    // tamanho dos slots
    fwrite(&rec_len,sizeof(int),1,f);
    qty_fill -= sizeof(int);
    // bitmap
    bitmap = malloc(slot_qty*sizeof(short));
    for (i=0;i<slot_qty;i++) {
        bitmap[i] = 0; // valor 0 slot vago
    }
    fwrite(bitmap,sizeof(short),slot_qty,f);

    // preencher o resto da página
    fill = malloc(qty_fill);
    for (i=0;i<qty_fill;i++)
        fill[i] = '#';
    fwrite(fill,1,qty_fill,f);

    fclose(f);

    rid.page = page;
    rid.slot = slot;

    return rid;
};

struct record_id find_rid(int page) {
    FILE *f;
    struct record_id rid;
    int i,bitmap_len,rec_len,slot=-1;
    char nextpage;
    short *bitmap;

    f=fopen("agenda.dat","r+b");

    if (f==NULL) {
        printf("File not found\n");
        exit(0);
    }

    fseek(f,page*PAGE_SIZE,SEEK_SET); // vamos até a página solicitada (inicia em 0)

    // obtemos o tamanho do bitmap, o bitmap, uma flag se ha proxima pagina e o tamanho dos registros
    fread(&nextpage,1,1,f);
    fread(&bitmap_len,sizeof(int),1,f);
    fread(&rec_len,sizeof(int),1,f);

    bitmap = malloc(bitmap_len*sizeof(short));
    fread(bitmap,sizeof(short),bitmap_len,f);

    // busca um slot disponivel
    for (i=0; i<bitmap_len; i++) {
        if (bitmap[i] == 0) {
            slot = i;
            break;
        }
    }

    // não encontramos slot vago nesta página
    if (slot == -1) {
        // temos proxima pagina?
        if (nextpage == 'Y') {
            // procuramos nela
            rid = find_rid(++page);
        } else {
            // criamos nova pagina
            rid = create_page(page, bitmap_len, rec_len);
            // atualizamos essa indicando que há outra depois dela
            nextpage = 'Y';
            fseek(f,page*PAGE_SIZE,SEEK_SET);
            fwrite(&nextpage,1,1,f);
        }
    } else {
        rid.page = page;
        rid.slot = slot;
    }

    fclose(f);

    return rid;
};

void insert() {
    FILE *f;
    struct theader *t;
    struct record_id rid;
    int i, page, slot,bitmap_len,rec_len,ivalue;
    double dvalue;
    char opt, buf[STRING_MAX],c,nextpage;
    short *bitmap,busy=1;

    t=readHeader();

    do {
        rid = find_rid(0); // busca pagina e slot para inserir

        page = rid.page;
        slot = rid.slot;

        f=fopen("agenda.dat","r+b");

        if (f==NULL) {
            printf("File not found\n");
            exit(0);
        }

        fseek(f,page*PAGE_SIZE,SEEK_SET); // vamos até a página

        // obtemos o tamanho do bitmap, o bitmap, uma flag se ha proxima pagina e o tamanho dos registros
        fread(&nextpage,1,1,f);
        fread(&bitmap_len,sizeof(int),1,f);
        fread(&rec_len,sizeof(int),1,f);

        bitmap = malloc(bitmap_len*sizeof(short));
        fread(bitmap,sizeof(short),bitmap_len,f);

        fseek(f,rec_len*slot,SEEK_CUR); // seek até a posicao do slot

        for (i=0;i<MFIELD && t[i].name[0]!='#';i++) {
            printf("\n%s :",t[i].name);
            switch (t[i].type) {
                case 'S': fgets(buf,t[i].len+1,stdin);
                          if (buf[strlen(buf)-1]!='\n')  c = getchar();
                          else buf[strlen(buf)-1]=0;
                          fwrite(buf,t[i].len,1,f);
                        break;
                case 'C': buf[0]=fgetc(stdin);
                          while((c = getchar()) != '\n' && c != EOF); /// garbage collector
                          fwrite(buf,t[i].len,1,f);
                        break;
                case 'I': scanf("%d",&ivalue);
                          while((c = getchar()) != '\n' && c != EOF); /// garbage collector
                          fwrite(&ivalue,t[i].len,1,f);
                        break;
                case 'F': scanf("%lf",&dvalue);
                          while((c = getchar()) != '\n' && c != EOF); /// garbage collector
                          fwrite(&dvalue,t[i].len,1,f);
                        break;
            }
        }

        fseek(f,page*PAGE_SIZE,SEEK_SET); // voltamos ao inicio da pagina
        fseek(f,1+sizeof(int)+sizeof(int),SEEK_CUR); // vamos ao bitmap
        fseek(f,slot*sizeof(short),SEEK_CUR); // vamos ao registro do slot
        fwrite(&busy,sizeof(short),1,f); // escrevemos o valor no bitmap
        fclose(f);

        printf("Continuar (S/N): "); opt=getchar();
        while((c = getchar()) != '\n' && c != EOF); /// garbage collector

    } while (opt=='S' || opt=='s');
}

void delete_record() {
    FILE *f;
    int i,page,slot,slot_flag,bitmap_len,rec_len,invalid;
    char opt,c,nextpage;
    short *bitmap,free=0;

    do {
        opt='s';
        printf("Página: ");
        scanf("%d", &page);
        printf("Slot: ");
        scanf("%d", &slot);
        getchar();

        f=fopen("agenda.dat","r+b");

        if (f==NULL) {
            printf("File not found\n");
            exit(0);
        }

        // verifica numeros de pagina e/ou slot invalidos
        if (page<0) {
            printf("Número de página inválido!\n");
            continue;
        } else if (slot<0) {
            printf("Número de slot inválido!\n");
            continue;
        }

        invalid = 0;
        // varificamos se a página solicitada existe
        for (i=0; i<page; i++) {
            fseek(f,i*PAGE_SIZE,SEEK_SET);
            fread(&nextpage,1,1,f);

            if (nextpage != 'Y') {
                invalid = 1;
                printf("A página solicitada não existe!\n");
                break;
            }
        }

        if (invalid == 1) continue;

        fseek(f,page*PAGE_SIZE,SEEK_SET); // vamos até a página

        // obtemos o tamanho do bitmap, o bitmap, uma flag se ha proxima pagina e o tamanho dos registros
        fread(&nextpage,1,1,f);
        fread(&bitmap_len,sizeof(int),1,f);

        if (!(bitmap_len>slot)) {
            printf("O slot solicitado não existe!\n");
            continue;
        }

        fread(&rec_len,sizeof(int),1,f);
        bitmap = malloc(bitmap_len*sizeof(short));
        fread(bitmap,sizeof(short),bitmap_len,f);

        slot_flag = bitmap[slot];

        if (slot_flag == 1) {
            fseek(f,page*PAGE_SIZE,SEEK_SET); // voltamos ao inicio da pagina
            fseek(f,1+sizeof(int)+sizeof(int),SEEK_CUR); // vamos ao bitmap
            fseek(f,slot*sizeof(short),SEEK_CUR); // vamos ao registro do slot
            fwrite(&free,sizeof(short),1,f); // escrevemos o valor no bitmap
            printf("Registro Página: %d Slot: %d excluído com sucesso!\n",page,slot);
        } else {
            printf("Registro Página: %d Slot: %d está livre. Nada foi feito.\n",page,slot);
        }

        fclose(f);

        printf("Continuar (S/N): "); opt=getchar();
        while((c = getchar()) != '\n' && c != EOF); /// garbage collector

    } while (opt=='S' || opt=='s');
}

void create_table() {
    char *fill, fname[15], ftype, option='S', nextpage='N';
    int i, flen, qty=0, rec_len=0, qty_fill, busy_slots, fields_len=(15+1+sizeof(int))*MFIELD, slot_qty=0;
    struct theader *th=(struct theader*) malloc(sizeof(struct theader)*MFIELD);
    FILE *f;
    short *bitmap;

    f = fopen("agenda.dat","w");

    if (f == NULL) {
        printf("File could not be created\n");
        exit(0);
    }

    printf("Informe os campos da tabela.\n");

    // solicita os campos do schema até no máximo 10 ou quando o usuario reesponder N
    while ((option=='S' || option=='s') && qty < MFIELD) {
        printf("Nome do campo: ");
        fgets(fname,15,stdin);
        printf("Tipo (string='S', char='C', integer='I', float='F'): ");
        scanf("%c", &ftype);

        if (fname[0] == '\n') {
            printf("Nome do campo deve ser preenchido!\n");
            continue;
        }

        // remove caractere da nova linha da string do nome
        strtok(fname, "\n");

        if (ftype == 'S') {
            printf("Tamanho (máx. 256 bytes): ");
            scanf("%d", &flen);
            if (flen > STRING_MAX)
                flen = STRING_MAX;
        } else if (ftype == 'C') {
            flen = 1;
        } else if (ftype == 'I') {
            flen = sizeof(int);
        } else if (ftype == 'F') {
            flen = sizeof(double);
        } else {
            printf("Tipo do campo inválido!\n");
            continue;
        }

        rec_len += flen;

        // adiciona os dados do campo ao vetor de structs
        strcpy(th[qty].name,fname);
        th[qty].type = ftype;
        th[qty].len = flen;
        qty++;

        // solicita se o usuario quer colocar mais um campo
        if (qty < MFIELD) {
            getchar();
            printf("Adicionar outro campo? S/N ");
            scanf("%c", &option);
            getchar();
        }
    }

    // gravamos o header na primeira página, após ela teremos um char que indica se tem proxima pagina
    // em seguida um int que indica o tamanho do bitmap e na sequencia o bitmap e os registros
    qty_fill = PAGE_SIZE;
    // quantidade de slots
    slot_qty = PAGE_SIZE/rec_len;
    slot_qty = (PAGE_SIZE-1-sizeof(int)-sizeof(int)-slot_qty)/rec_len;
    qty_fill -= fields_len;
    // flag proxima pagina
    fwrite(&nextpage,1,1,f);
    qty_fill -= 1;
    // numero de slots (tamanho do bitmap)
    fwrite(&slot_qty,sizeof(int),1,f);
    qty_fill -= sizeof(int);
    // tamanho dos slots
    fwrite(&rec_len,sizeof(int),1,f);
    qty_fill -= sizeof(int);
    // bitmap
    busy_slots = (fields_len + (rec_len - 1))/rec_len; // slots inutilizados ocupados pelo header, arredondado para cima
    bitmap = malloc(slot_qty*sizeof(short));
    for (i=0;i<slot_qty;i++) {
        if(i<busy_slots) {
            bitmap[i] = 2; // valor 2 inutiliza o slot
        } else {
            bitmap[i] = 0; // 0 = slot livre
        }
    }
    fwrite(bitmap,sizeof(short),slot_qty,f);
    qty_fill -= slot_qty*sizeof(short);

    for (i=0; i<qty; i++) {
        // escreve as informacoes do schema na pagina
        fwrite(th[i].name,15,1,f);
        fwrite(&th[i].type,1,1,f);
        fwrite(&th[i].len,sizeof(int),1,f);
    }

    // preencher com # caso haja menos de 10 campos
    if (qty < MFIELD) {
        strcpy(fname,"#");
        fwrite(fname,15+1+sizeof(int),MFIELD-qty,f);
    }

    // preencher o resto da página
    fill = malloc(qty_fill);
    for (i=0;i<qty_fill;i++)
        fill[i] = '#';
    fwrite(fill,1,qty_fill,f);

    fclose(f);
}

void selectAll(int page) {
    FILE *f;
    struct theader *t;
    int i,j,k,space,bitmap_len,rec_len,ibuf;
    double dbuf;
    char buf[STRING_MAX], nextpage;
    short *bitmap;

    t=readHeader();
    f=fopen("agenda.dat","r");

    if (f==NULL) {
        printf("File not found\n");
        exit(0);
    }

    fseek(f,page*PAGE_SIZE,SEEK_SET); // vamos até a página solicitada (inicia em 0)

    // obtemos o tamanho do bitmap, o bitmap, uma flag se ha proxima pagina e o tamanho dos registros
    fread(&nextpage,1,1,f);
    fread(&bitmap_len,sizeof(int),1,f);
    fread(&rec_len,sizeof(int),1,f);

    bitmap = malloc(bitmap_len*sizeof(short));
    fread(bitmap,sizeof(short),bitmap_len,f);

    printf("Página: %d\n",page);

    // se for a primeira pagina imprime o cabecalho da tabela
    if (page == 0) {
        for (i=0; i<MFIELD && t[i].name[0]!='#'; i++) {
            printf("%s ",t[i].name);
            space=t[i].len-strlen(t[i].name);
            for (j=1;j<=space;j++)
                printf(" ");
        }
        printf("\n");
    }

    // percorre o bitmap e imprime somente os registros com a flag '1' (ocupado)
    for (k=0; k<bitmap_len; k++) {
        if (bitmap[k] == 1) {
            for (i=0; i<MFIELD && t[i].name[0]!='#'; i++) {
                switch (t[i].type) {
                    case 'S':
                        if (!fread(buf,t[i].len,1,f)) break;

                        for (j=0;j<t[i].len && buf[j]!=0;j++)
                            printf("%c",buf[j]);
                        space=t[i].len-j;
                        for (j=0;j<=space;j++)
                            printf(" ");
                        break;

                    case 'C':
                        if (!fread(buf,t[i].len,1,f)) break;

                        printf("%c ",buf[0]);
                        break;

                    case 'I':
                        if (!fread(&ibuf,t[i].len,1,f)) break;

                        printf("%d",ibuf);
                        break;
                    case 'F':
                        if (!fread(&dbuf,t[i].len,1,f)) break;

                        printf("%lf",dbuf);
                        break;
                }
            }
            printf(" Slot: %d\n",k);
        } else {
            fseek(f,rec_len,SEEK_CUR);
        }
    }

    fclose(f);

    // se tiver proxima pagina, chama a funcao novamente
    if (nextpage == 'Y') {
        selectAll(page+1);
    }
}

int main() {
	int opcao;
	do {
		printf("\n\n   SIMULADOR PAGINAS BANCO DE DADOS \n\n");
		printf(" 1 - Estruturar paginas \n");
		printf(" 2 - Inserir Registros\n");
		printf(" 3 - Mostrar registros\n");
		printf(" 4 - Excluir registro\n\n");
		printf(" 0 - Sair\n");
		printf("\n Escolha: ");

        scanf("%d",&opcao);
        getchar();

		switch(opcao){
			case 0:
				return 0;
			case 1:
				create_table();
				break;
			case 2:
				insert();
				break;
			case 3:
				selectAll(0);
				break;
			case 4:
				delete_record();
				break;
			default:
				printf("\n\n   Opcao Invalida!   \n\n");
		}
	}while(opcao != 0);
    return 0;
}
