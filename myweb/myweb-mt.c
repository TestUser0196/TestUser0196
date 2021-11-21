#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//--------------- for thread -------------------
#include <pthread.h>
#include <poll.h>

//--------------- for journal -------------------
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
//---------------- for mmap -------------------

#include <sys/mman.h>

//-----------------------------------------------

#define HTTP_HEADER_LEN 256
#define HTTP_REQUEST_LEN 256
#define HTTP_METHOD_LEN 6
#define HTTP_URI_LEN 100

#define REQ_END 100
#define ERR_NO_URI -100
#define ERR_ENDLESS_URI -101

#define FILE_NAME_LEN 1000

struct http_req {
	char request[HTTP_REQUEST_LEN];
	char method[HTTP_METHOD_LEN];
	char uri[HTTP_URI_LEN];
	char uri_path[HTTP_URI_LEN];
	char uri_params[HTTP_URI_LEN];
	// version
	// user_agent
	// server
	// accept
};

int fill_req(char *buf, struct http_req *req) {
	if (strlen(buf) == 2) {
		// пустая строка (\r\n) означает конец запроса
		return REQ_END;
	}

	// ловим uri path
	/*	char * first;
		char * last;
		first = strchr(buf, '/');
		last = strchr(buf, '?');
		int len_uri_path = 1;
		if(last !=NULL && first !=NULL){
		   len_uri_path = (last - first)/sizeof(char); 
			// ловим uri path
			strncpy(req->uri_path, first, len_uri_path);
			// ловим uri_params
			strncpy(req->uri_params, last, HTTP_URI_LEN - 1);
		}*/
	// --------------

	char *p, *a, *b;
	// Это строка GET-запроса
	p = strstr(buf, "GET");
	if (p == buf) {
		// Строка запроса должна быть вида
		// GET /dir/ HTTP/1.0
		// GET /dir HTTP/1.1
		// GET /test123?r=123 HTTP/1.1
		// и т.п.
		strncpy(req->request, buf, strlen(buf));
		req->request[strlen(buf) /*+ 1*/] = 0;
		strncpy(req->method, "GET", strlen("GET"));
		a = strchr(buf, '/');

		if ( a != NULL) { // есть запрашиваемый URI 
			b = strchr(a, ' ');
			if ( b != NULL ) { // конец URI
				strncpy(req->uri, a, b-a);
				b = strchr(a, '?');
				if(b != NULL ){
					strncpy(req->uri_path  , &a[1], b-a-1);
					req->uri_path[b-a]=0;
					strncpy(req->uri_params, b, HTTP_URI_LEN - 1);
				}
				else{
					 b = strchr(a, 'H');
					 strncpy(req->uri_path  , &a[1], b-a/*-1*/); //2
					 req->uri_path[b-a-2]=0; //1
				}

			} else {
				return ERR_ENDLESS_URI;  
				// тогда это что-то не то
			}

		} else {
			return ERR_NO_URI; 
			// тогда это что-то не то
		}
	}

	return 0;	
}

const char *logfile   = "/var/log/myweb/access.log";
const char *base_path = "/srv/myweb/"/*"/home/sysadmin/unix-dev-sys/myweb/"*/;

int write_to_journal(char *entry) {
	
	int fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND, 0666);
	
	struct tm *local;
	time_t t;
	t = time(NULL);
	local = localtime(&t);
	write(fd, "Data: ", strlen("Data: "));
	write(fd,asctime(local), strlen(asctime(local)));
	write(fd, entry, strlen(entry));
	write(fd, "\n", 1);
	fsync(fd);
	close(fd);
	return 0;
}

int log_req(struct http_req *req) {
	// fprintf(stderr, "%s %s\n%s\n", req->request, req->method, req->uri);
	/*	
	if (access(logfile,F_OK|W_OK) == 0 )
		write(1,"File exists\n",12);
	else 
		write(1, "No file\n",8);  */
	return write_to_journal(req->request);

	//return 0;
}

int make_resp(struct http_req *req) {
	

	/*printf("HTTP/1.1 200 OK\r\n");
	printf("Content-Type: text/html\r\n");
	printf("\r\n");*/

	int fdin;
    	struct stat statbuf;
    	void *mmf_ptr;
	// определяем на основе запроса, что за файл открыть
	char res_file[FILE_NAME_LEN] = "";
	strncpy(res_file,base_path,strlen(base_path));
	
	if(strcmp(req->uri_path, "file1.html") == 0 || strcmp(req->uri_path, "file2.html") == 0
       || strcmp(req->uri_path, "") == 0){
		if(strcmp(req->uri_path, "") == 0)
			strcat(res_file,"StartPage.html");
		else
			strcat(res_file,req->uri_path);
		/*printf("<html><body><title>Page title</title><h1>Page Header TestUser 0196</h1>");
		printf("<p>URI PATH: %s<p>",res_file);
		printf("</body></html>\r\n");*/
		// открываем
        	if ((fdin=open(res_file, O_RDONLY)) < 0) {
                	perror(res_file);
                	return 1;
        	}
		// размер
        	if (fstat(fdin, &statbuf) < 0) {
                	perror(res_file);
                	return 1;
        	}
		// mmf
        	if ((mmf_ptr = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fdin, 0)) == MAP_FAILED) {
                	perror("myfile");
                	return 1;
        	}

		// Выводим HTTP-заголовки
		char *http_result = "HTTP/1.1 200 OK\r\n";
		write(1,http_result,strlen(http_result));
		char *http_contype = "Content-Type: text/html\r\n";
		write(1,http_contype,strlen(http_contype));
		char *header_end = "\r\n";
		write(1,header_end,strlen(header_end));
		// Выводим запрошенный ресурс
        	if (write(1,mmf_ptr,statbuf.st_size) != statbuf.st_size) {
                	perror("stdout");
                	return 1;
        	}

		/*random_data buf;
		buf.state = NULL;
		initstate_r(unsigned int seed, char *statebuf,
                       size_t statelen, &buf);*/
		short a,b;
		if(req->uri_path[12] != 0){
			a=random()/req->uri_path[12];
			b=random()/req->uri_path[12];
		}
		else{
			a=random()/1000000;
			b=random()/1000000;
		}
		printf("<p>Culculate somesing ... %d ...<p>", a+b);

		// Подчищаем ресурсы
        	close(fdin);
        	munmap(mmf_ptr,statbuf.st_size);
	
	}
	/*else if(strcmp(req->uri_path, "file2.html") == 0){
		printf("<html><body><title>Page title</title><h1>Page Header TestUser 0196</h1>");
		printf("<p>URI PATH: FILE 2<p>");
		printf("</body></html>\r\n");
		
	}*/
	else{
		printf("<html><body><title>Page title</title><h1>Page Header TestUser 0196</h1>");
		printf("<p>Invalid request :<p>");		
		printf("<p>URI PATH: %s<p>",req->uri_path);
		printf("<p>Code of strcmp(uri_path, file1.html): %d<p>",strcmp(req->uri_path, "/file1.html"));
		printf("</body></html>\r\n");
	}

	return 0;
}

struct http_req req;

#define N_MAX_THR 10
pthread_t thr_id[N_MAX_THR];

unsigned char cur_thr = 0;
unsigned char cur_ent = 0;

void* thr_func(void* arg) {

	unsigned char cnt=0;
	
	/*while(cnt<25){ 
		cnt++;
		printf("Working thread %i\r\n", cur_thr);
	}*/
	
	make_resp(&req);
	printf("Working thread %i\r\n", cur_thr);

	return NULL;

}

char test = 0;
char test1 = 0;

int main (void) {
	char buf[HTTP_HEADER_LEN];
	//struct http_req req;
	struct pollfd fds[2];  // будем отслеживать 2 канала ввод и вывод
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[1].fd = STDOUT_FILENO;
	fds[1].events = POLLIN;
	
	while(1){
		int res = poll(fds, 2, 10000);
		if(res > 0){
			char buf[100];
			int bytes = 0;
			/*
			if ( fds[0].revents & POLLIN ) {
				bytes = read(fds[0].fd, buf, 100);
				if(bytes>0)
					printf("\r\n Poll worked0\r\n");
				fds[0].revents = 0;
			}
      		if ( fds[1].revents & POLLIN ) {
				bytes = read(fds[1].fd, buf, 100);
				if(bytes>0)
					printf("\r\n Poll worked1\r\n");
				fds[1].revents = 0;
			}*/
			
			while(fgets(buf, sizeof(buf),stdin)) {
				int ret = fill_req(buf, &req);
		
				cur_ent++;
				test1 = 1;
				
				if (ret == 0) 
					// строка запроса обработана, переходим к следующей
					continue;
				if (ret == REQ_END ){ 
					// конец HTTP запроса, вываливаемся на обработку
					//создаём очередной поток
				
					break;
				}
				else
					// какая-то ошибка 
					printf("Error: %d\n", ret);
		
			}
		
			log_req(&req);
			if(test1 == 1){
				make_resp(&req);
				test1 = 0;
			}
			
			if(cur_thr<9 && 0){
				test = 1;			
				pthread_create(&thr_id[cur_thr], NULL, &thr_func,NULL);
				cur_thr++;
			}
	
			if(test == 1){
				printf("\r\nWas create thread %i\r\n", cur_thr);
				test = 0;
			}
		
			if(test1 == 1){
				printf("Enter to while (...) ... %i\r\n", cur_ent);
				test1 = 0;
			}
		
			//pthread_join(thr_id[cur_thr-1], NULL);
		}
		
	}
	
	return 0;
}
