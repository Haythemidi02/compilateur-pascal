#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ID 32
#define MAX_TABLE 100

// Codes tokens
#define TOK_PROGRAM 1
#define TOK_ID 2
#define TOK_SEMI 3
#define TOK_VAR 4
#define TOK_COLON 5
#define TOK_COMMA 6
#define TOK_INTEGER 7
#define TOK_CHAR 8
#define TOK_BEGIN 9
#define TOK_END 10
#define TOK_DOT 11
#define TOK_ASSIGN 12
#define TOK_IF 13
#define TOK_THEN 14
#define TOK_ELSE 15
#define TOK_WHILE 16
#define TOK_DO 17
#define TOK_READ 18
#define TOK_READLN 19
#define TOK_WRITE 20
#define TOK_WRITELN 21
#define TOK_LPAR 22
#define TOK_RPAR 23
#define TOK_NB 24

// Opérateurs relationnels
#define TOK_OPREL_EQ 30
#define TOK_OPREL_NEQ 31
#define TOK_OPREL_LT 32
#define TOK_OPREL_LE 33
#define TOK_OPREL_GT 34
#define TOK_OPREL_GE 35

// Opérateurs additifs
#define TOK_PLUS 40
#define TOK_MINUS 41
#define TOK_OR 42

// Opérateurs multiplicatifs
#define TOK_MUL 50
#define TOK_DIV 51
#define TOK_MOD 52
#define TOK_AND 53

#define TOK_EOF 99

// Types pour l'analyse sémantique
typedef enum {
    TYPE_INTEGER,
    TYPE_CHAR,
    TYPE_BOOL,
    TYPE_ERROR,
    TYPE_NONE
} Type;

// Instructions de la machine abstraite
typedef enum {
    IC_LIT,     // LIT n : empile la constante n
    IC_LOD,     // LOD a : empile la valeur de l'adresse a
    IC_STO,     // STO a : dépile et stocke à l'adresse a
    IC_ADD,     // ADD : addition
    IC_SUB,     // SUB : soustraction
    IC_MUL,     // MUL : multiplication
    IC_DIV,     // DIV : division
    IC_MOD,     // MOD : modulo
    IC_EQ,      // EQ : égalité
    IC_NEQ,     // NEQ : différent
    IC_LT,      // LT : inférieur
    IC_LE,      // LE : inférieur ou égal
    IC_GT,      // GT : supérieur
    IC_GE,      // GE : supérieur ou égal
    IC_JMP,     // JMP adr : saut inconditionnel
    IC_JF,      // JF adr : saut si faux
    IC_READ,    // READ : lecture
    IC_WRITE,   // WRITE : écriture
    IC_WRITELN, // WRITELN : écriture avec saut de ligne
    IC_HALT     // HALT : arrêt
} InstructionCode;

// Structure pour une instruction
typedef struct {
    InstructionCode op;
    int arg;
    int besoin_backpatch;
} Instruction;

// Structure pour la table des symboles
typedef struct {
    char nom[MAX_ID];
    Type type;
    int adresse;
    int initialise;
} SymboleTable;

// Structure pour stocker le token
typedef struct {
    int code;
    char id[MAX_ID];
    int val;
    int ligne;
} Token;

// Variables globales
Token token_courant;
FILE *fichier;
int ligne = 1;
int erreur_semantique = 0;

SymboleTable table_symboles[MAX_TABLE];
int nb_symboles = 0;
int adresse_courante = 0;

Instruction code_intermediaire[1000];
int ic_index = 0;

// Liste pour stocker temporairement les identifiants
char id_list[10][MAX_ID];
int id_count = 0;

// Mots-clés
struct {
    char *mot;
    int token;
} mots_cles[] = {
    {"program", TOK_PROGRAM}, {"var", TOK_VAR}, {"integer", TOK_INTEGER},
    {"char", TOK_CHAR}, {"begin", TOK_BEGIN}, {"end", TOK_END},
    {"if", TOK_IF}, {"then", TOK_THEN}, {"else", TOK_ELSE},
    {"while", TOK_WHILE}, {"do", TOK_DO},
    {"read", TOK_READ}, {"readln", TOK_READLN},
    {"write", TOK_WRITE}, {"writeln", TOK_WRITELN},
    {NULL, 0}
};

// ===== FONCTIONS =====

void gen_code(InstructionCode op, int arg) {
    if (ic_index >= 1000) {
        printf("Erreur: débordement table instructions\n");
        exit(1);
    }
    code_intermediaire[ic_index].op = op;
    code_intermediaire[ic_index].arg = arg;
    code_intermediaire[ic_index].besoin_backpatch = 0;
    ic_index++;
}

int gen_code_backpatch(InstructionCode op) {
    if (ic_index >= 1000) {
        printf("Erreur: débordement table instructions\n");
        exit(1);
    }
    code_intermediaire[ic_index].op = op;
    code_intermediaire[ic_index].arg = 0;
    code_intermediaire[ic_index].besoin_backpatch = 1;
    return ic_index++;
}

void backpatch(int index, int valeur) {
    if (index >= 0 && index < ic_index) {
        code_intermediaire[index].arg = valeur;
        code_intermediaire[index].besoin_backpatch = 0;
    }
}

int rechercher_symbole(char *nom) {
    for (int i = 0; i < nb_symboles; i++) {
        if (strcmp(table_symboles[i].nom, nom) == 0) {
            return i;
        }
    }
    return -1;
}

int ajouter_symbole(char *nom, Type type) {
    if (rechercher_symbole(nom) != -1) {
        printf("Erreur sémantique ligne %d: '%s' déjà déclaré\n", ligne, nom);
        erreur_semantique = 1;
        return -1;
    }

    if (nb_symboles >= MAX_TABLE) {
        printf("Erreur: table des symboles pleine\n");
        exit(1);
    }

    strcpy(table_symboles[nb_symboles].nom, nom);
    table_symboles[nb_symboles].type = type;
    table_symboles[nb_symboles].adresse = nb_symboles;  // Index = adresse
    table_symboles[nb_symboles].initialise = 0;

    return nb_symboles++;
}

Type obtenir_type_symbole(char *nom) {
    int index = rechercher_symbole(nom);
    if (index == -1) {
        printf("Erreur sémantique ligne %d: '%s' non déclaré\n", ligne, nom);
        erreur_semantique = 1;
        return TYPE_ERROR;
    }
    return table_symboles[index].type;
}

int obtenir_adresse(char *nom) {
    int index = rechercher_symbole(nom);
    if (index == -1) return -1;
    return table_symboles[index].adresse;
}

void marquer_initialise(char *nom) {
    int index = rechercher_symbole(nom);
    if (index != -1) {
        table_symboles[index].initialise = 1;
    }
}

void verifier_initialise(char *nom) {
    int index = rechercher_symbole(nom);
    if (index != -1 && !table_symboles[index].initialise) {
        printf("Avertissement ligne %d: '%s' non initialisée\n", ligne, nom);
    }
}

Type verifier_type_binaire(Type t1, Type t2, char* operation) {
    if (t1 == TYPE_ERROR || t2 == TYPE_ERROR) return TYPE_ERROR;

    if (t1 != t2) {
        printf("Erreur sémantique ligne %d: types incompatibles dans '%s'\n", ligne, operation);
        erreur_semantique = 1;
        return TYPE_ERROR;
    }

    if (strstr(operation, "relation")) {
        return TYPE_BOOL;
    }

    return t1;
}

Token AnalLex() {
    int c;
    char buffer[MAX_ID];
    int i = 0;

    while ((c = fgetc(fichier)) != EOF) {
        if (c == ' ' || c == '\t') continue;
        if (c == '\n') { ligne++; continue; }

        if (c == '(') {
            int c2 = fgetc(fichier);
            if (c2 == '*') {
                while ((c = fgetc(fichier)) != EOF) {
                    if (c == '*') {
                        c = fgetc(fichier);
                        if (c == ')') break;
                        if (c == EOF) {
                            printf("Erreur: commentaire non fermé ligne %d\n", ligne);
                            exit(1);
                        }
                        ungetc(c, fichier);
                    }
                    if (c == '\n') ligne++;
                }
                continue;
            } else {
                ungetc(c2, fichier);
            }
        }
        break;
    }

    if (c == EOF) {
        token_courant.code = TOK_EOF;
        token_courant.ligne = ligne;
        return token_courant;
    }

    if (isalpha(c)) {
        buffer[0] = c; i = 1;
        while ((c = fgetc(fichier)) != EOF && (isalnum(c) || c == '_')) {
            if (i < MAX_ID-1) buffer[i++] = c;
        }
        buffer[i] = '\0';
        ungetc(c, fichier);

        for (int k = 0; mots_cles[k].mot != NULL; k++) {
            if (strcmp(buffer, mots_cles[k].mot) == 0) {
                token_courant.code = mots_cles[k].token;
                token_courant.ligne = ligne;
                return token_courant;
            }
        }
        token_courant.code = TOK_ID;
        strcpy(token_courant.id, buffer);
        token_courant.ligne = ligne;
        return token_courant;
    }

    if (isdigit(c)) {
        int val = 0;
        while (isdigit(c)) {
            val = val * 10 + (c - '0');
            c = fgetc(fichier);
        }
        ungetc(c, fichier);
        token_courant.code = TOK_NB;
        token_courant.val = val;
        token_courant.ligne = ligne;
        return token_courant;
    }

    int c2 = fgetc(fichier);
    if (c == '<') {
        if (c2 == '=') {
            token_courant.code = TOK_OPREL_LE;
            token_courant.ligne = ligne;
            return token_courant;
        }
        if (c2 == '>') {
            token_courant.code = TOK_OPREL_NEQ;
            token_courant.ligne = ligne;
            return token_courant;
        }
        ungetc(c2, fichier);
        token_courant.code = TOK_OPREL_LT;
        token_courant.ligne = ligne;
        return token_courant;
    }
    if (c == '>') {
        if (c2 == '=') {
            token_courant.code = TOK_OPREL_GE;
            token_courant.ligne = ligne;
            return token_courant;
        }
        ungetc(c2, fichier);
        token_courant.code = TOK_OPREL_GT;
        token_courant.ligne = ligne;
        return token_courant;
    }
    if (c == ':' && c2 == '=') {
        token_courant.code = TOK_ASSIGN;
        token_courant.ligne = ligne;
        return token_courant;
    }
    if (c == '|' && c2 == '|') {
        token_courant.code = TOK_OR;
        token_courant.ligne = ligne;
        return token_courant;
    }
    if (c == '&' && c2 == '&') {
        token_courant.code = TOK_AND;
        token_courant.ligne = ligne;
        return token_courant;
    }
    ungetc(c2, fichier);

    switch (c) {
        case ';': token_courant.code = TOK_SEMI; break;
        case ':': token_courant.code = TOK_COLON; break;
        case ',': token_courant.code = TOK_COMMA; break;
        case '.': token_courant.code = TOK_DOT; break;
        case '(': token_courant.code = TOK_LPAR; break;
        case ')': token_courant.code = TOK_RPAR; break;
        case '+': token_courant.code = TOK_PLUS; break;
        case '-': token_courant.code = TOK_MINUS; break;
        case '*': token_courant.code = TOK_MUL; break;
        case '/': token_courant.code = TOK_DIV; break;
        case '=': token_courant.code = TOK_OPREL_EQ; break;
        case '%': token_courant.code = TOK_MOD; break;
        default:
            printf("Erreur lexicale ligne %d: caractère illégal '%c' (%d)\n", ligne, c, c);
            exit(1);
    }
    token_courant.ligne = ligne;
    return token_courant;
}

void init_analex(char *nom_fichier) {
    fichier = fopen(nom_fichier, "r");
    if (!fichier) {
        printf("Erreur : impossible d'ouvrir %s\n", nom_fichier);
        exit(1);
    }
    ligne = 1;
    nb_symboles = 0;
    ic_index = 0;
    erreur_semantique = 0;
    id_count = 0;
}

void afficher_code_intermediaire() {
    printf("\n=== CODE INTERMEDIAIRE ===\n");
    for (int i = 0; i < ic_index; i++) {
        printf("%3d: ", i);
        switch (code_intermediaire[i].op) {
            case IC_LIT: printf("LIT %d\n", code_intermediaire[i].arg); break;
            case IC_LOD: printf("LOD %d\n", code_intermediaire[i].arg); break;
            case IC_STO: printf("STO %d\n", code_intermediaire[i].arg); break;
            case IC_ADD: printf("ADD\n"); break;
            case IC_SUB: printf("SUB\n"); break;
            case IC_MUL: printf("MUL\n"); break;
            case IC_DIV: printf("DIV\n"); break;
            case IC_MOD: printf("MOD\n"); break;
            case IC_EQ: printf("EQ\n"); break;
            case IC_NEQ: printf("NEQ\n"); break;
            case IC_LT: printf("LT\n"); break;
            case IC_LE: printf("LE\n"); break;
            case IC_GT: printf("GT\n"); break;
            case IC_GE: printf("GE\n"); break;
            case IC_JMP: printf("JMP %d\n", code_intermediaire[i].arg); break;
            case IC_JF: printf("JF %d\n", code_intermediaire[i].arg); break;
            case IC_READ: printf("READ\n"); break;
            case IC_WRITE: printf("WRITE\n"); break;
            case IC_WRITELN: printf("WRITELN\n"); break;
            case IC_HALT: printf("HALT\n"); break;
        }
    }
}

void afficher_table_symboles() {
    printf("\n=== TABLE DES SYMBOLES ===\n");
    printf("Nom\t\tType\t\tAdresse\tInitialisé\n");
    printf("------------------------------------------------\n");
    for (int i = 0; i < nb_symboles; i++) {
        printf("%-12s\t", table_symboles[i].nom);
        switch (table_symboles[i].type) {
            case TYPE_INTEGER: printf("INTEGER\t\t"); break;
            case TYPE_CHAR: printf("CHAR\t\t"); break;
            case TYPE_BOOL: printf("BOOL\t\t"); break;
            default: printf("ERROR\t\t");
        }
        printf("%d\t\t", table_symboles[i].adresse);
        printf("%s\n", table_symboles[i].initialise ? "OUI" : "NON");
    }
}