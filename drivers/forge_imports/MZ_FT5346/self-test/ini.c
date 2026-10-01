#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/unistd.h>

#include "ini.h"

char CFG_SSL = '[';  /* Ïî±êÖ¾·ûSection Symbol --¿É¸ù¾ÝÌØÊâÐèÒª½øÐÐ¶¨Òå¸ü¸Ä£¬Èç { }µÈ*/
char CFG_SSR = ']';  /* Ïî±êÖ¾·ûSection Symbol --¿É¸ù¾ÝÌØÊâÐèÒª½øÐÐ¶¨Òå¸ü¸Ä£¬Èç { }µÈ*/
char CFG_NIS = ':';  /* name Óë index Ö®¼äµÄ·Ö¸ô·û */
char CFG_NTS = '#';  /* ×¢ÊÍ·û*/

char * ini_str_trim_r(char * buf);
char * ini_str_trim_l(char * buf);
int ini_file_get_line(char *filedata, char *buffer, int maxlen); 
int ini_split_key_value(char *buf, char **key, char **val); 
long atol(char *nptr);


/*************************************************************
Function: »ñµÃkeyµÄÖµ
Input: char * filedata¡¡ÎÄ¼þ£»char * section¡¡ÏîÖµ£»char * key¡¡¼üÖµ
Output: char * value¡¡keyµÄÖµ
Return: 0		SUCCESS
-1		Î´ÕÒµ½section
-2		Î´ÕÒµ½key
-10		ÎÄ¼þ´ò¿ªÊ§°Ü
-12		¶ÁÈ¡ÎÄ¼þÊ§°Ü
-14		ÎÄ¼þ¸ñÊ½´íÎó
-22		³¬³ö»º³åÇø´óÐ¡
Note: 
*************************************************************/
int ini_get_key(char *filedata, char * section, char * key, char * value)
{
	char buf1[MAX_CFG_BUF + 1];
	char buf2[MAX_CFG_BUF + 1];
	//char *buf1 = NULL;
	//char *buf2 = NULL;
	char *key_ptr, *val_ptr;
	int  n, ret;
	int dataoff = 0;

	//buf1 = kmalloc(MAX_CFG_BUF + 1, GFP_KERNEL);
	//buf2 = kmalloc(MAX_CFG_BUF + 1, GFP_KERNEL);

	*value='\0';

	while(1) { /* ËÑÕÒÏîsection */
		ret = CFG_ERR_READ_FILE;
		n = ini_file_get_line(filedata+dataoff, buf1, MAX_CFG_BUF);
		dataoff += n;
		if(n < -1)
			goto r_cfg_end;
		ret = CFG_SECTION_NOT_FOUND;
		if(n < 0)
			goto r_cfg_end; /* ÎÄ¼þÎ²£¬Î´·¢ÏÖ */ 

		if(n > MAX_CFG_BUF)
			goto r_cfg_end;

		n = strlen(ini_str_trim_l(ini_str_trim_r(buf1)));
		if(n == 0 || buf1[0] == CFG_NTS)
			continue;       /* ¿ÕÐÐ »ò ×¢ÊÍÐÐ */ 

		ret = CFG_ERR_FILE_FORMAT;
		if(n > 2 && ((buf1[0] == CFG_SSL && buf1[n-1] != CFG_SSR)))
			goto r_cfg_end;
		if(buf1[0] == CFG_SSL) {
			buf1[n-1] = 0x00;
			if(strcmp(buf1+1, section) == 0)
			{
				break; /* ÕÒµ½Ïîsection */
			}
		} 
	} 

	while(1){ /* ËÑÕÒkey */ 
		ret = CFG_ERR_READ_FILE;
		n = ini_file_get_line(filedata+dataoff, buf1, MAX_CFG_BUF);
		dataoff += n;
		if(n < -1) 
			goto r_cfg_end;
		ret = CFG_KEY_NOT_FOUND;
		if(n < 0)
			goto r_cfg_end;/* ÎÄ¼þÎ²£¬Î´·¢ÏÖkey */ 
		n = strlen(ini_str_trim_l(ini_str_trim_r(buf1)));
		if(n == 0 || buf1[0] == CFG_NTS) 
			continue;       /* ¿ÕÐÐ »ò ×¢ÊÍÐÐ */ 
		ret = CFG_KEY_NOT_FOUND; 
		if(buf1[0] == CFG_SSL)
		{
			goto r_cfg_end;
		}
		if(buf1[n-1] == '+') { /* Óö+ºÅ±íÊ¾ÏÂÒ»ÐÐ¼ÌÐø  */ 		
			buf1[n-1] = 0x00; 
			while(1) {			
				ret = CFG_ERR_READ_FILE; 
				n = ini_file_get_line(filedata+dataoff, buf2, MAX_CFG_BUF);
				dataoff += n;
				if(n < -1) 
					goto r_cfg_end; 
				if(n < 0) 
					break;/* ÎÄ¼þ½áÊø */ 

				n = strlen(ini_str_trim_r(buf2)); 
				ret = CFG_ERR_EXCEED_BUF_SIZE; 
				if(n > 0 && buf2[n-1] == '+'){/* Óö+ºÅ±íÊ¾ÏÂÒ»ÐÐ¼ÌÐø */ 
					buf2[n-1] = 0x00; 
					if( (strlen(buf1) + strlen(buf2)) > MAX_CFG_BUF) 
						goto r_cfg_end; 
					strcat(buf1, buf2); 
					continue; 
				} 
				if(strlen(buf1) + strlen(buf2) > MAX_CFG_BUF) 
					goto r_cfg_end; 
				strcat(buf1, buf2); 
				break; 
			} 
		} 
		ret = CFG_ERR_FILE_FORMAT; 
		if(ini_split_key_value(buf1, &key_ptr, &val_ptr) != 1) 
			goto r_cfg_end; 

		ini_str_trim_l(ini_str_trim_r(key_ptr)); 
		if(strcmp(key_ptr, key) != 0) 
			continue;                                  /* ºÍkeyÖµ²»Æ¥Åä */ 
		strcpy(value, val_ptr); 
		break; 
	} 
	ret = CFG_OK; 
r_cfg_end:
	return ret; 
} 
/*************************************************************
Function: »ñµÃËùÓÐsection
Input:  char *filename¡¡ÎÄ¼þ,int max ×î´ó¿É·µ»ØµÄsectionµÄ¸öÊý
Output: char *sections[]¡¡´æ·ÅsectionÃû×Ö
Return: ·µ»Øsection¸öÊý¡£Èô³ö´í£¬·µ»Ø¸ºÊý¡£
-10			ÎÄ¼þ´ò¿ª³ö´í
-12			ÎÄ¼þ¶ÁÈ¡´íÎó
-14			ÎÄ¼þ¸ñÊ½´íÎó
Note: 
*************************************************************/
int ini_get_sections(char *filedata, unsigned char * sections[], int max)
{
	//FILE *fp; 
	char buf1[MAX_CFG_BUF + 1]; 
	int n, n_sections = 0, ret; 
	int dataoff = 0;

	while(1) {/*ËÑÕÒÏîsection */
		ret = CFG_ERR_READ_FILE;
		n = ini_file_get_line(filedata+dataoff, buf1, MAX_CFG_BUF);
		dataoff += n;
		if(n < -1) 
			goto cfg_scts_end; 
		if(n < 0)
			break;/* ÎÄ¼þÎ² */ 
		n = strlen(ini_str_trim_l(ini_str_trim_r(buf1)));
		if(n == 0 || buf1[0] == CFG_NTS) 
			continue;       /* ¿ÕÐÐ »ò ×¢ÊÍÐÐ */ 
		ret = CFG_ERR_FILE_FORMAT;
		if(n > 2 && ((buf1[0] == CFG_SSL && buf1[n-1] != CFG_SSR)))
			goto cfg_scts_end;
		if(buf1[0] == CFG_SSL) {
			if (max!=0){
				buf1[n-1] = 0x00;
				strcpy((char *)sections[n_sections], buf1+1);
				if (n_sections>=max)
					break;		/* ³¬¹ý¿É·µ»Ø×î´ó¸öÊý */
			}
			n_sections++;
		} 

	} 
	ret = n_sections;
cfg_scts_end: 
	//	if(fp != NULL)
	//		fclose(fp);
	return ret;
} 


/*************************************************************
Function: È¥³ý×Ö·û´®ÓÒ±ßµÄ¿Õ×Ö·û
Input:  char * buf ×Ö·û´®Ö¸Õë
Output: 
Return: ×Ö·û´®Ö¸Õë
Note: 
*************************************************************/
char * ini_str_trim_r(char * buf)
{
	int len,i;
	char tmp[512];

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf);

	memset(tmp,0x00,len);
	for(i = 0;i < len;i++) {
		if (buf[i] !=' ')
			break;
	}
	if (i < len) {
		strncpy(tmp,(buf+i),(len-i));
	}
	strncpy(buf,tmp,len);
	return buf;
}

/*************************************************************
Function: È¥³ý×Ö·û´®×ó±ßµÄ¿Õ×Ö·û
Input:  char * buf ×Ö·û´®Ö¸Õë
Output: 
Return: ×Ö·û´®Ö¸Õë
Note: 
*************************************************************/
char * ini_str_trim_l(char * buf)
{
	int len,i;	
	char tmp[512];

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf);

	memset(tmp,0x00,len);

	for(i = 0;i < len;i++) {
		if (buf[len-i-1] !=' ')
			break;
	}
	if (i < len) {
		strncpy(tmp,buf,len-i);
	}
	strncpy(buf,tmp,len);
	return buf;
}
/*************************************************************
Function: ´ÓÎÄ¼þÖÐ¶ÁÈ¡Ò»ÐÐ
Input:  FILE *fp ÎÄ¼þ¾ä±ú£»int maxlen »º³åÇø×î´ó³¤¶È
Output: char *buffer Ò»ÐÐ×Ö·û´®
Return: >0		Êµ¼Ê¶ÁµÄ³¤¶È
-1		ÎÄ¼þ½áÊø
-2		¶ÁÎÄ¼þ³ö´í
Note: 
*************************************************************/
int ini_file_get_line(char *filedata, char *buffer, int maxlen)
{
	int  i, j; 
	char ch1; 

	for(i=0, j=0; i<maxlen; j++) { 
		ch1 = filedata[j];
		if(ch1 == '\n' || ch1 == 0x00) 
			break; /* »»ÐÐ */ 
		if(ch1 == '\f' || ch1 == 0x1A) {      /* '\f':»»Ò³·ûÒ²ËãÓÐÐ§×Ö·û */ 			
			buffer[i++] = ch1; 
			break; 
		}
		if(ch1 != '\r') buffer[i++] = ch1;    /* ºöÂÔ»Ø³µ·û */ 
	} 
	buffer[i] = '\0'; 
	return i+2; 
} 
/*************************************************************
Function: ·ÖÀëkeyºÍvalue
key=val
jack   =   liaoyuewang 
|      |   | 
k1     k2  i 
Input:  char *buf
Output: char **key, char **val
Return: 1 --- ok 
0 --- blank line 
-1 --- no key, "= val" 
-2 --- only key, no '=' 
Note: 
*************************************************************/
int  ini_split_key_value(char *buf, char **key, char **val)
{
	int  i, k1, k2, n; 

	if((n = strlen((char *)buf)) < 1)
		return 0; 
	for(i = 0; i < n; i++) 
		if(buf[i] != ' ' && buf[i] != '\t')
			break; 

	if(i >= n)
		return 0;

	if(buf[i] == '=')
		return -1;

	k1 = i;
	for(i++; i < n; i++) 
		if(buf[i] == '=') 
			break;

	if(i >= n)
		return -2;
	k2 = i;

	for(i++; i < n; i++)
		if(buf[i] != ' ' && buf[i] != '\t') 
			break; 

	buf[k2] = '\0'; 

	*key = buf + k1; 
	*val = buf + i; 
	return 1; 
} 

int my_atoi(const char *str)
{
	int result = 0;
	int signal = 1; /* Ä¬ÈÏÎªÕýÊý */
	if((*str>='0'&&*str<='9')||*str=='-'||*str=='+') {
		if(*str=='-'||*str=='+') { 
			if(*str=='-')
				signal = -1; /*ÊäÈë¸ºÊý*/
			str++;
		}
	}
	else 
		return 0;
	/*¿ªÊ¼×ª»»*/
	while(*str>='0' && *str<='9')
		result = result*10 + (*str++ - '0' );

	return signal*result;
}

int isspace(int x)  
{  
	if(x==' '||x=='\t'||x=='\n'||x=='\f'||x=='\b'||x=='\r')  
		return 1;  
	else   
		return 0;  
}  

int isdigit(int x)  
{  
	if(x<='9' && x>='0')           
		return 1;   
	else   
		return 0;  
} 

long atol(char *nptr)
{
	int c; /* current char */
	long total; /* current total */
	int sign; /* if ''-'', then negative, otherwise positive */
	/* skip whitespace */
	while ( isspace((int)(unsigned char)*nptr) )
		++nptr;
	c = (int)(unsigned char)*nptr++;
	sign = c; /* save sign indication */
	if (c == '-' || c == '+')
		c = (int)(unsigned char)*nptr++; /* skip sign */
	total = 0;
	while (isdigit(c)) {
		total = 10 * total + (c - '0'); /* accumulate digit */
		c = (int)(unsigned char)*nptr++; /* get next char */
	}
	if (sign == '-')
		return -total;
	else
		return total; /* return result, negated if necessary */
}
/***
*int atoi(char *nptr) - Convert string to long
*
*Purpose:
* Converts ASCII string pointed to by nptr to binary.
* Overflow is not detected. Because of this, we can just use
* atol().
*
*Entry:
* nptr = ptr to string to convert
*
*Exit:
* return int value of the string
*
*Exceptions:
* None - overflow is not detected.
*
*******************************************************************************/
int atoi(char *nptr)
{
	return (int)atol(nptr);
}

