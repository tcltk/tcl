typedef struct ExprSlot {
    int type;     /* refers to the type values in TclGetNumberFromObj */
    void *value;
} ExprSlot;

typedef struct ExprData {
    int pc;
    int numSlots;
    ExprSlot slot[1]; /* will be grown */
} ExprData;


/* Opcodes used only in expressions */
#define INST_JUMP4			14
#define INST_JUMP_TRUE4			15
#define INST_JUMP_FALSE4		16
#define INST_BITOR			17
#define INST_BITXOR			18
#define INST_BITAND			19
#define INST_EQ				20
#define INST_NEQ			21
#define INST_LT				22
#define INST_GT				23
#define INST_LE				24
#define INST_GE				25
#define INST_LSHIFT			26
#define INST_RSHIFT			27
#define INST_ADD			28
#define INST_SUB			29
#define INST_MULT			30
#define INST_DIV			31
#define INST_MOD			32
#define INST_UPLUS			33
#define INST_UMINUS			34
#define INST_BITNOT			35
#define INST_LNOT			36
#define INST_EXPON			37

#define INST_STR_EQ			38
#define INST_STR_NEQ			39

#define INST_LIST_IN			40
#define INST_LIST_NOT_IN		41

#define INST_TRY_CVT_TO_NUMERIC		42
#define INST_REVERSE			43
/* The last opcode */
#define LAST_INST_OPCODE		43
