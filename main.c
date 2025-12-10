#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "analex.c"

Token token;

void erreur(const char *msg) {
    printf("Erreur syntaxique ligne %d : %s (token=%d)\n", token.ligne, msg, token.code);
    exit(1);
}

void accepter(int T) {
    if (token.code == T)
        token = AnalLex();
    else {
        char buf[100];
        sprintf(buf, "token attendu %d, reçu %d", T, token.code);
        erreur(buf);
    }
}

// Déclarations
void P();
void Dcl();
void Dcl_prime();
void Liste_id();
Type Type_spec();
void Inst_composee();
void Inst();
void Liste_inst();
void Liste_inst_prime();
Type I();
Type Exp();
Type Exp_prime();
Type Exp_simple();
Type ExpS_prime();
Type Terme();
Type Terme_prime();
Type Facteur();

// ===== FONCTIONS SYNTAXIQUES =====

void P() {
    accepter(TOK_PROGRAM);
    if (token.code != TOK_ID)
        erreur("identificateur attendu après program");
    printf("Programme : %s\n", token.id);
    accepter(TOK_ID);
    accepter(TOK_SEMI);
    Dcl();
    Inst_composee();
    accepter(TOK_DOT);

    gen_code(IC_HALT, 0);

    if (!erreur_semantique) {
        printf("\nAnalyse syntaxique et sémantique terminée avec succès !\n");
        afficher_table_symboles();
        afficher_code_intermediaire();
    } else {
        printf("\nAnalyse terminée avec des erreurs sémantiques.\n");
    }
}

void Dcl() {
    Dcl_prime();
}

void Dcl_prime() {
    if (token.code == TOK_VAR) {
        accepter(TOK_VAR);

        id_count = 0;
        Liste_id();
        accepter(TOK_COLON);

        Type type_var = Type_spec();

        for (int i = 0; i < id_count; i++) {
            ajouter_symbole(id_list[i], type_var);
        }

        accepter(TOK_SEMI);
        Dcl_prime();
    }
    // ε
}

void Liste_id() {
    if (token.code != TOK_ID)
        erreur("identificateur attendu");

    strcpy(id_list[id_count++], token.id);
    accepter(TOK_ID);

    while (token.code == TOK_COMMA) {
        accepter(TOK_COMMA);
        if (token.code != TOK_ID)
            erreur("identificateur attendu après virgule");

        strcpy(id_list[id_count++], token.id);
        accepter(TOK_ID);
    }
}

Type Type_spec() {
    if (token.code == TOK_INTEGER) {
        accepter(TOK_INTEGER);
        return TYPE_INTEGER;
    }
    else if (token.code == TOK_CHAR) {
        accepter(TOK_CHAR);
        return TYPE_CHAR;
    }
    else {
        erreur("type attendu (integer | char)");
        return TYPE_ERROR;
    }
}

void Inst_composee() {
    accepter(TOK_BEGIN);
    Inst();
    accepter(TOK_END);
}

void Inst() {
    if (token.code == TOK_ID || token.code == TOK_IF || token.code == TOK_WHILE ||
        token.code == TOK_READ || token.code == TOK_READLN ||
        token.code == TOK_WRITE || token.code == TOK_WRITELN) {
        Liste_inst();
    }
    // ε
}

void Liste_inst() {
    I();
    Liste_inst_prime();
}

void Liste_inst_prime() {
    if (token.code == TOK_SEMI) {
        accepter(TOK_SEMI);
        I();
        Liste_inst_prime();
    }
    // ε
}

Type I() {
    Type type_retour = TYPE_NONE;

    if (token.code == TOK_ID) {
        char nom_var[MAX_ID];
        strcpy(nom_var, token.id);
        Type type_var = obtenir_type_symbole(nom_var);
        int adr = obtenir_adresse(nom_var);

        accepter(TOK_ID);
        accepter(TOK_ASSIGN);

        Type type_exp = Exp_simple();

        if (type_var != TYPE_ERROR && type_exp != TYPE_ERROR && type_var != type_exp) {
            printf("Erreur sémantique ligne %d: type incompatible dans l'affectation\n", token.ligne);
            erreur_semantique = 1;
        }

        if (adr != -1) {
            gen_code(IC_STO, adr);
        }

        marquer_initialise(nom_var);
        type_retour = type_var;
    }
    else if (token.code == TOK_IF) {
        accepter(TOK_IF);

        Type type_cond = Exp();
        if (type_cond != TYPE_BOOL && type_cond != TYPE_ERROR) {
            printf("Erreur sémantique ligne %d: condition doit être booléenne\n", token.ligne);
            erreur_semantique = 1;
        }

        // JF avec backpatch
        int jf_index = gen_code_backpatch(IC_JF);

        accepter(TOK_THEN);
        I();

        // JMP avec backpatch
        int jmp_index = gen_code_backpatch(IC_JMP);

        // Backpatch JF vers ici
        backpatch(jf_index, ic_index);

        accepter(TOK_ELSE);
        I();

        // Backpatch JMP vers ici
        backpatch(jmp_index, ic_index);

        type_retour = TYPE_NONE;
    }
    else if (token.code == TOK_WHILE) {
        int debut_boucle = ic_index;

        accepter(TOK_WHILE);

        Type type_cond = Exp();
        if (type_cond != TYPE_BOOL && type_cond != TYPE_ERROR) {
            printf("Erreur sémantique ligne %d: condition doit être booléenne\n", token.ligne);
            erreur_semantique = 1;
        }

        // JF avec backpatch
        int jf_index = gen_code_backpatch(IC_JF);

        accepter(TOK_DO);
        I();

        // JMP vers le début
        gen_code(IC_JMP, debut_boucle);

        // Backpatch JF vers ici
        backpatch(jf_index, ic_index);

        type_retour = TYPE_NONE;
    }
    else if (token.code == TOK_READ || token.code == TOK_READLN) {
        int est_readln = (token.code == TOK_READLN);
        accepter(token.code);
        accepter(TOK_LPAR);

        if (token.code != TOK_ID)
            erreur("identificateur attendu");

        char nom_var[MAX_ID];
        strcpy(nom_var, token.id);
        Type type_var = obtenir_type_symbole(nom_var);
        int adr = obtenir_adresse(nom_var);

        accepter(TOK_ID);
        accepter(TOK_RPAR);

        gen_code(IC_READ, 0);
        if (adr != -1) {
            gen_code(IC_STO, adr);
        }
        if (est_readln) {
            gen_code(IC_WRITELN, 0);
        }

        marquer_initialise(nom_var);
        type_retour = TYPE_NONE;
    }
    else if (token.code == TOK_WRITE || token.code == TOK_WRITELN) {
        int est_writeln = (token.code == TOK_WRITELN);
        accepter(token.code);
        accepter(TOK_LPAR);

        if (token.code != TOK_ID)
            erreur("identificateur attendu");

        char nom_var[MAX_ID];
        strcpy(nom_var, token.id);
        verifier_initialise(nom_var);
        Type type_var = obtenir_type_symbole(nom_var);
        int adr = obtenir_adresse(nom_var);

        accepter(TOK_ID);
        accepter(TOK_RPAR);

        if (adr != -1) {
            gen_code(IC_LOD, adr);
        }
        gen_code(IC_WRITE, 0);
        if (est_writeln) {
            gen_code(IC_WRITELN, 0);
        }

        type_retour = TYPE_NONE;
    }
    else {
        erreur("instruction attendue");
        type_retour = TYPE_ERROR;
    }

    return type_retour;
}

Type Exp() {
    Type type1 = Exp_simple();
    Type type2 = Exp_prime();

    if (type2 == TYPE_NONE) {
        return type1;
    } else {
        return TYPE_BOOL;
    }
}

Type Exp_prime() {
    if (token.code >= TOK_OPREL_EQ && token.code <= TOK_OPREL_GE) {
        int op_token = token.code;
        accepter(op_token);

        Exp_simple();

        InstructionCode op_code;
        switch (op_token) {
            case TOK_OPREL_EQ: op_code = IC_EQ; break;
            case TOK_OPREL_NEQ: op_code = IC_NEQ; break;
            case TOK_OPREL_LT: op_code = IC_LT; break;
            case TOK_OPREL_LE: op_code = IC_LE; break;
            case TOK_OPREL_GT: op_code = IC_GT; break;
            case TOK_OPREL_GE: op_code = IC_GE; break;
            default: op_code = IC_EQ;
        }
        gen_code(op_code, 0);

        return TYPE_BOOL;
    } else {
        return TYPE_NONE;
    }
}

Type Exp_simple() {
    Type type1 = Terme();
    Type type2 = ExpS_prime();

    if (type2 == TYPE_NONE) {
        return type1;
    } else {
        return verifier_type_binaire(type1, type2, "arithmetique");
    }
}

Type ExpS_prime() {
    if (token.code == TOK_PLUS || token.code == TOK_MINUS || token.code == TOK_OR) {
        int op_token = token.code;
        accepter(op_token);

        Type type2 = Terme();

        InstructionCode op_code;
        switch (op_token) {
            case TOK_PLUS: op_code = IC_ADD; break;
            case TOK_MINUS: op_code = IC_SUB; break;
            case TOK_OR: op_code = IC_ADD; break;
            default: op_code = IC_ADD;
        }
        gen_code(op_code, 0);

        if (op_token == TOK_OR) {
            return TYPE_BOOL;
        }
        return type2;
    } else {
        return TYPE_NONE;
    }
}

Type Terme() {
    Type type1 = Facteur();
    Type type2 = Terme_prime();

    if (type2 == TYPE_NONE) {
        return type1;
    } else {
        return verifier_type_binaire(type1, type2, "multiplicatif");
    }
}

Type Terme_prime() {
    if (token.code == TOK_MUL || token.code == TOK_DIV ||
        token.code == TOK_MOD || token.code == TOK_AND) {
        int op_token = token.code;
        accepter(op_token);

        Type type2 = Facteur();

        InstructionCode op_code;
        switch (op_token) {
            case TOK_MUL: op_code = IC_MUL; break;
            case TOK_DIV: op_code = IC_DIV; break;
            case TOK_MOD: op_code = IC_MOD; break;
            case TOK_AND: op_code = IC_MUL; break;
            default: op_code = IC_MUL;
        }
        gen_code(op_code, 0);

        if (op_token == TOK_AND) {
            return TYPE_BOOL;
        }
        return type2;
    } else {
        return TYPE_NONE;
    }
}

Type Facteur() {
    Type type_retour = TYPE_ERROR;

    if (token.code == TOK_ID) {
        char nom_var[MAX_ID];
        strcpy(nom_var, token.id);
        type_retour = obtenir_type_symbole(nom_var);
        verifier_initialise(nom_var);
        int adr = obtenir_adresse(nom_var);

        if (adr != -1) {
            gen_code(IC_LOD, adr);
        }

        accepter(TOK_ID);
    }
    else if (token.code == TOK_NB) {
        type_retour = TYPE_INTEGER;
        gen_code(IC_LIT, token.val);
        accepter(TOK_NB);
    }
    else if (token.code == TOK_LPAR) {
        accepter(TOK_LPAR);
        type_retour = Exp_simple();
        accepter(TOK_RPAR);
    }
    else {
        erreur("facteur attendu (id | nb | (exp_simple))");
        type_retour = TYPE_ERROR;
    }

    return type_retour;
}

int main() {
    init_analex("test.pas");

    if (fichier == NULL) {
        printf("Erreur : impossible d'ouvrir 'test.pas'\n");
        return 1;
    }

    token = AnalLex();
    P();
    fclose(fichier);

    return 0;
}