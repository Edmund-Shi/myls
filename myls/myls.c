//a simple ls demo 
//support -1aA options
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>


#define MAX(X,Y) ((X)>(Y)?(X):(Y)) 

enum filetype {
	unknown,
	fifo,
	chardev,
	directory,
	blockdev,
	normal,
	symbolic_link,
	sock,
	whiteout,
	arg_directory
};
typedef enum { false, true } bool;
/* Display letters and indicators for each filetype.
Keep these in sync with enum filetype.  */
static char const filetype_letter[] = "?pcdb-lswd";

/*隐藏以.开始的项目,如果true,则-a开启*/
static bool _a_not_ignore_dot;

//显示方式 
enum display_patern {
	multi_items_per_line, // default
	one_item_per_line // -1 每行显示一个项目	
} display_patern;

// -A 显示除 “.”和“..”外的所有文件
static bool _A_ignore_dotdot;

//记录所有等待被显示的文件
struct pending_file
{
	struct dirent *dir;
	struct pending_file *next;
};
struct pending_file *pending_file; //第一个节点是空节点

								   //记录排序方式
enum sort_type
{
	sort_none = -1,		/* -U */
	sort_name,			/* default */
	sort_extension,		/* -X */
	sort_size,			/* -S */
	sort_version,		/* -v */
	sort_time,			/* -t */
	sort_numtypes		/* the number of elements of this enum */
};
static enum sort_type sort_type;

//分列相关
//每行最多15列
#define MAX_COL_NUM 15 
struct output_format {
	int col_num; // 列数,从1开始
	int row_num; // 行数,从1开始
	int max_single_col_len[MAX_COL_NUM]; //每列占用的宽度,从1开始
	int accumulate_col_position[MAX_COL_NUM]; // 每列的开始位置，可以通过上一行计算得出
};
struct output_format output_format;

static int tab_size = 2; //默认的间隔宽度

void print_dir(const char *dir_name); //打印指定目录
void initial_pattern(void); //初始化各种参数
char get_option(char* argv); //获取输入的参数
void readdir_to_pending(char * dir_name); //将目录读取到pending链表中
void sort_pending(void); // sort pending files
void parse_options(char* options); // 将用户输入的选项转化为相应的参数
void getTerminatorSize(int *cols, int *lines); // 获取终端的行数和列数
void calculate_single_col_len(void); //计算每列的宽度
int calculate_length_of_filename_and_frills(struct dirent *file); // 计算响应文件占用的长度
struct pending_file *move_pointer_n_times(struct pending_file *p, int n); //移动n次
void print_file_name_and_frills(struct dirent *dir, int pos); //输出单个数据

int main(int argc, char const *argv[])
{
	initial_pattern();

	if (argc == 1) { // ls 无参数
		print_dir(".");
	}
	else if (argc == 2) {
		//只含有目录 或 选项
		if (argv[1][0] == '-') { // ls -option
			parse_options(argv[1]);
			print_dir(".");
		}
		else { // ls dir
			print_dir(argv[1]);
		}

	}
	else {
		//ls [-option] [dir]
		parse_options(argv[1]);
		print_dir(argv[2]);
	}
	return 0;
}
void print_dir(const char *dir_name) {
	DIR *dir_p;
	struct dirent *dir;
	char seprator = '\t'; //相邻两元素的分隔符 默认为tab

	readdir_to_pending(dir_name);
	sort_pending(); //根据选项排序待定文件
	calculate_single_col_len(); //计算分列格式

	struct pending_file *cur_file_node_ptr; // 指向pending队列中当前的元素
	cur_file_node_ptr = pending_file;
	if (display_patern == multi_items_per_line) {
		struct pending_file *row_ptr = pending_file->next;
		for (int row = 0; row<output_format.row_num; row++) {
			int col = 0;
			struct pending_file *col_ptr = row_ptr;

			//print a single row
			while (1) {
				print_file_name_and_frills(col_ptr->dir, output_format.max_single_col_len[col] + tab_size);
				col++;
				col_ptr = move_pointer_n_times(col_ptr, output_format.row_num);
				if (col_ptr == NULL) {
					break;
				}
			}
			putchar('\n');
			row_ptr = row_ptr->next;
		}
		/*
		for(int col = 0;col<output_format.col_num;col++){
		printf("%c[%dD",0x1b,200); //向左移到行首
		printf("%c[%dC",0x1b,output_format.accumulate_col_position[col]); //移动到当前列的起始位置
		for(int row = 0;row < output_format.row_num;row++){
		if(cur_file_node_ptr -> next != NULL){
		cur_file_node_ptr = cur_file_node_ptr -> next;
		//printf("%c[s",0x1b);
		printf("%s",cur_file_node_ptr->dir->d_name);
		//printf("%c[u",0x1b);
		printf("\033[%dD",calculate_length_of_filename_and_frills(cur_file_node_ptr -> dir)); //move left
		}
		printf("%c[%dB",0x1b,1 ); //下移一行光标
		}
		printf("%c[%dA",0x1b,output_format.row_num); //上移至第一行
		}
		printf("%c[%dD",0x1b,200); //光标向左移到行首
		printf("%c[%dB",0x1b,output_format.row_num); //移动至最下面
		*/
	}

	else { //单行显示
		while (cur_file_node_ptr = cur_file_node_ptr->next) {
			seprator = '\n'; // -1 option

			printf("%s%c", cur_file_node_ptr->dir->d_name, seprator);
		}
	}
}
void initial_pattern(void) {
	_a_not_ignore_dot = false;
	display_patern = multi_items_per_line;
	_A_ignore_dotdot = false;
	pending_file = (struct pending_file *)malloc(sizeof(struct pending_file));
	pending_file->next = NULL;
	pending_file->dir = NULL;
	sort_type = sort_name;
}
char get_option(char* argv) {
	static int i = 1;
	return argv[i++];
}
void readdir_to_pending(char * dir_name) {
	DIR *dir_p;
	struct dirent *dir;
	dir_p = opendir(dir_name);
	struct pending_file * pending_tail;
	pending_tail = pending_file; // 链表尾
	while (dir = readdir(dir_p)) {
		if ((!_A_ignore_dotdot && !_a_not_ignore_dot) && dir->d_name[0] == '.') {
			// -a option
			continue;
		}
		if ((_A_ignore_dotdot && !strcmp(dir->d_name, "..")) || (_A_ignore_dotdot && !strcmp(dir->d_name, "."))) {
			// -A option 
			continue;
		}

		//将节点添加到pending队列中
		pending_tail->next = (struct pending_file*)malloc(sizeof(struct pending_file));
		pending_tail = pending_tail->next;
		pending_tail->dir = dir;
		pending_tail->next = NULL;
	}
	closedir(dir_p);
}
void parse_options(char* options) {
	char op;
	while (op = get_option(options)) {
		switch (op) {
		case 'a': _a_not_ignore_dot = true; break;
		case '1': display_patern = one_item_per_line; break;
		case 'A': _A_ignore_dotdot = true; break;
		default: break;
		}
	}
}
void sort_pending(void) {
	int num_of_total_files = 0;
	struct pending_file *ptr_pending;
	ptr_pending = pending_file; //临时指针

								//计数总共有多少待处理的文件
	while (ptr_pending->next != NULL) {
		num_of_total_files++;
		ptr_pending = ptr_pending->next;
	}

	//开始排序 
	//按照名字排序 - 选择排序
	if (sort_type == sort_name) {
		// default sort type
		struct pending_file *pfirst, *ptail; //新建的临时链表，临时链表没有空节点头
		struct pending_file *pminBefore, *pmin, *p; // p 当前比较的节点

		ptr_pending = pending_file;
		pfirst = ptail = NULL;
		while (ptr_pending->next != NULL) {
			//寻找最小值
			for (p = ptr_pending->next, pmin = ptr_pending->next, pminBefore = ptr_pending; p->next != NULL; p = p->next) {
				if (strcmp(p->next->dir->d_name, pmin->dir->d_name) < 0) { //找到一个比当前min小的节点(p->next)
					pminBefore = p;
					pmin = p->next;
				}
			}

			//将最小值插入临时链表，并从旧链表中删除
			if (pfirst == NULL) {
				pfirst = pmin;
				ptail = pmin;
			}
			else {
				ptail->next = pmin;
				ptail = pmin;
			}
			pminBefore->next = pmin->next;
		}
		ptail->next = NULL;
		pending_file->next = pfirst;
	}
	// sort by name finished
}

// 得到终端的列数和行数  
void getTerminatorSize(int *cols, int *lines) {
	char temp[5];
	strcpy(temp,getenv("LINES"));
	*lines = atoi(temp);
	strcpy(temp, getenv("COLUMNS")) ;
	*cols = atoi(temp);
#ifdef TIOCGSIZE    
	struct ttysize ts;
	ioctl(STDIN_FILENO, TIOCGSIZE, &ts);
	*cols = ts.ts_cols;
	*lines = ts.ts_lines;
#elif defined(TIOCGWINSZ)    
	struct winsize ts;
	ioctl(STDIN_FILENO, TIOCGWINSZ, &ts);
	*cols = ts.ws_col;
	*lines = ts.ws_row;
#endif /* TIOCGSIZE */      
}

void calculate_single_col_len(void) {
	// the size of terminal
	int cols = 80;
	int lines = 24;
	getTerminatorSize(&cols,&lines);
	int col_num = 1;
	int row_num = 1;
	while (1) { //每次循环实验一个row_num
		int free_char = cols; //剩余可用的字符数
		int max_len_local = 0; //当前列最长的字符占用
		struct pending_file *current_file = pending_file; //指向当前打印的文件
		col_num = 1;
		while (1) { //循环完成所有列的打印
			int temp_len;
			max_len_local = 0;
			for (int i = 0; i<row_num; i++) { //打印一列中的内容
				if (current_file->next == NULL) {
					break;
				}
				current_file = current_file->next;
				temp_len = calculate_length_of_filename_and_frills(current_file->dir);
				max_len_local = MAX(max_len_local, temp_len);
			}
			output_format.max_single_col_len[col_num - 1] = max_len_local; // 记录当前列最宽的宽度
			free_char -= max_len_local;			// 剩余的字符数减小			
			if (free_char <= 0 || current_file->next == NULL) {
				break;
			}
			col_num++;							// 下一列
			free_char -= tab_size;
		}
		if (free_char <= 0) { //并未成功打印完全
			row_num++;
		}
		else {
			output_format.col_num = col_num;
			output_format.row_num = row_num;
			break; 		//找到了合适的宽度
		}
	}
	output_format.accumulate_col_position[0] = 0;
	for (int i = 1; i<output_format.col_num; i++) {
		output_format.accumulate_col_position[i] = output_format.accumulate_col_position[i - 1] + \
			output_format.max_single_col_len[i - 1] + tab_size;
	}
}
int calculate_length_of_filename_and_frills(struct dirent *file) {
	// 计算响应文件占用的长度
	int res_len;
	res_len = strlen(file->d_name);
	return res_len;
}
struct pending_file *move_pointer_n_times(struct pending_file *p, int n) {
	for (int i = 0; i<n; i++) {
		if (p != NULL) {
			p = p->next;
		}
	}
	return p;
}
void print_file_name_and_frills(struct dirent *dir, int pos) {
	printf("%*s", -pos, dir->d_name);
}