typedef	unsigned int	u32;
typedef	unsigned char	u8;

typedef struct {
	u32	S[4][256];
	u32	P[18];
} blf_ctx;

void blf_enc(blf_ctx *c, u32 *data, int blocks);
void blf_dec(blf_ctx *c, u32 *data, int blocks);
void blf_key(blf_ctx *c, unsigned char *key, int len);

