/*all-config*/
#define TIME 1000
#define NUMBER 2
#define SIGNAL 9
#define MAX_STRING 64

/*memory_leak-config*/
#define Memory_Leak 80

/*fork-congig*/
#define MAX_NUMBER 1000

/*Detection function*/
extern void memory_leak(void);
extern void fork_max(void);

/*Recovery function*/
extern void sig_kill(struct task_struct *p);
extern void name_sig_kill(char name[]);
