#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define MASK_BIT(var,mask) ((var) & (mask))

#define SET_FLAG(var,flag) ((var) | (flag))
#define CLEAR_FLAG(var,flag) ((var) & ~(flag))
#define CHECK_FLAG(var,flag) ((var) & (flag))
