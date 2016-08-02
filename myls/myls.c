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

/*������.��ʼ����Ŀ,���true,��-a����*/
static bool _a_not_ignore_dot;

//��ʾ��ʽ 
enum display_patern {
	multi_items_per_line, // default
	one_item_per_line // -1 ÿ����ʾһ����Ŀ	
} display_patern;

// -A ��ʾ�� ��.���͡�..����������ļ�
static bool _A_ignore_dotdot;

//��¼���еȴ�����ʾ���ļ�
struct pending_file
{
	struct dirent *dir;
	struct pending_file *next;
};
struct pending_file *pending_file; //��һ���ڵ��ǿսڵ�

								   //��¼����ʽ
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

//�������
//ÿ�����15��
#define MAX_COL_NUM 15 
struct output_format {
	int col_num; // ����,��1��ʼ
	int row_num; // ����,��1��ʼ
	int max_single_col_len[MAX_COL_NUM]; //ÿ��ռ�õĿ��,��1��ʼ
	int accumulate_col_position[MAX_COL_NUM]; // ÿ�еĿ�ʼλ�ã�����ͨ����һ�м���ó�
};
struct output_format output_format;

static int tab_size = 2; //Ĭ�ϵļ�����

void print_dir(const char *dir_name); //��ӡָ��Ŀ¼
void initial_pattern(void); //��ʼ�����ֲ���
char get_option(char* argv); //��ȡ����Ĳ���
void readdir_to_pending(char * dir_name); //��Ŀ¼��ȡ��pending������
void sort_pending(void); // sort pending files
void parse_options(char* options); // ���û������ѡ��ת��Ϊ��Ӧ�Ĳ���
void getTerminatorSize(int *cols, int *lines); // ��ȡ�ն˵�����������
void calculate_single_col_len(void); //����ÿ�еĿ��
int calculate_length_of_filename_and_frills(struct dirent *file); // ������Ӧ�ļ�ռ�õĳ���
struct pending_file *move_pointer_n_times(struct pending_file *p, int n); //�ƶ�n��
void print_file_name_and_frills(struct dirent *dir, int pos); //�����������

int main(int argc, char const *argv[])
{
	initial_pattern();

	if (argc == 1) { // ls �޲���
		print_dir(".");
	}
	else if (argc == 2) {
		//ֻ����Ŀ¼ �� ѡ��
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
	char seprator = '\t'; //������Ԫ�صķָ��� Ĭ��Ϊtab

	readdir_to_pending(dir_name);
	sort_pending(); //����ѡ����������ļ�
	calculate_single_col_len(); //������и�ʽ

	struct pending_file *cur_file_node_ptr; // ָ��pending�����е�ǰ��Ԫ��
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
		printf("%c[%dD",0x1b,200); //�����Ƶ�����
		printf("%c[%dC",0x1b,output_format.accumulate_col_position[col]); //�ƶ�����ǰ�е���ʼλ��
		for(int row = 0;row < output_format.row_num;row++){
		if(cur_file_node_ptr -> next != NULL){
		cur_file_node_ptr = cur_file_node_ptr -> next;
		//printf("%c[s",0x1b);
		printf("%s",cur_file_node_ptr->dir->d_name);
		//printf("%c[u",0x1b);
		printf("\033[%dD",calculate_length_of_filename_and_frills(cur_file_node_ptr -> dir)); //move left
		}
		printf("%c[%dB",0x1b,1 ); //����һ�й��
		}
		printf("%c[%dA",0x1b,output_format.row_num); //��������һ��
		}
		printf("%c[%dD",0x1b,200); //��������Ƶ�����
		printf("%c[%dB",0x1b,output_format.row_num); //�ƶ���������
		*/
	}

	else { //������ʾ
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
	pending_tail = pending_file; // ����β
	while (dir = readdir(dir_p)) {
		if ((!_A_ignore_dotdot && !_a_not_ignore_dot) && dir->d_name[0] == '.') {
			// -a option
			continue;
		}
		if ((_A_ignore_dotdot && !strcmp(dir->d_name, "..")) || (_A_ignore_dotdot && !strcmp(dir->d_name, "."))) {
			// -A option 
			continue;
		}

		//���ڵ���ӵ�pending������
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
	ptr_pending = pending_file; //��ʱָ��

								//�����ܹ��ж��ٴ�������ļ�
	while (ptr_pending->next != NULL) {
		num_of_total_files++;
		ptr_pending = ptr_pending->next;
	}

	//��ʼ���� 
	//������������ - ѡ������
	if (sort_type == sort_name) {
		// default sort type
		struct pending_file *pfirst, *ptail; //�½�����ʱ������ʱ����û�пսڵ�ͷ
		struct pending_file *pminBefore, *pmin, *p; // p ��ǰ�ȽϵĽڵ�

		ptr_pending = pending_file;
		pfirst = ptail = NULL;
		while (ptr_pending->next != NULL) {
			//Ѱ����Сֵ
			for (p = ptr_pending->next, pmin = ptr_pending->next, pminBefore = ptr_pending; p->next != NULL; p = p->next) {
				if (strcmp(p->next->dir->d_name, pmin->dir->d_name) < 0) { //�ҵ�һ���ȵ�ǰminС�Ľڵ�(p->next)
					pminBefore = p;
					pmin = p->next;
				}
			}

			//����Сֵ������ʱ�������Ӿ�������ɾ��
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

// �õ��ն˵�����������  
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
	while (1) { //ÿ��ѭ��ʵ��һ��row_num
		int free_char = cols; //ʣ����õ��ַ���
		int max_len_local = 0; //��ǰ������ַ�ռ��
		struct pending_file *current_file = pending_file; //ָ��ǰ��ӡ���ļ�
		col_num = 1;
		while (1) { //ѭ����������еĴ�ӡ
			int temp_len;
			max_len_local = 0;
			for (int i = 0; i<row_num; i++) { //��ӡһ���е�����
				if (current_file->next == NULL) {
					break;
				}
				current_file = current_file->next;
				temp_len = calculate_length_of_filename_and_frills(current_file->dir);
				max_len_local = MAX(max_len_local, temp_len);
			}
			output_format.max_single_col_len[col_num - 1] = max_len_local; // ��¼��ǰ�����Ŀ��
			free_char -= max_len_local;			// ʣ����ַ�����С			
			if (free_char <= 0 || current_file->next == NULL) {
				break;
			}
			col_num++;							// ��һ��
			free_char -= tab_size;
		}
		if (free_char <= 0) { //��δ�ɹ���ӡ��ȫ
			row_num++;
		}
		else {
			output_format.col_num = col_num;
			output_format.row_num = row_num;
			break; 		//�ҵ��˺��ʵĿ��
		}
	}
	output_format.accumulate_col_position[0] = 0;
	for (int i = 1; i<output_format.col_num; i++) {
		output_format.accumulate_col_position[i] = output_format.accumulate_col_position[i - 1] + \
			output_format.max_single_col_len[i - 1] + tab_size;
	}
}
int calculate_length_of_filename_and_frills(struct dirent *file) {
	// ������Ӧ�ļ�ռ�õĳ���
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