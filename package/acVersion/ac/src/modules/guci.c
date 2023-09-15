#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include <uci.h>

#include "guci.h"

//static struct uci_context *ctx=NULL;
static const char *delimiter = " ";
enum {
	/* section cmds */
	CMD_GET,
	CMD_SET,
	CMD_ADD_LIST,
	CMD_DEL_LIST,
	CMD_DEL,
	CMD_RENAME,
	CMD_REVERT,
	CMD_REORDER,
	/* package cmds */
	CMD_SHOW,
	CMD_CHANGES,
	CMD_EXPORT,
	CMD_COMMIT,
	/* other cmds */
	CMD_ADD,
	CMD_IMPORT,
	CMD_HELP,
};
//static enum {
	//CLI_FLAG_MERGE =    (1 << 0),
	//CLI_FLAG_QUIET =    (1 << 1),
	//CLI_FLAG_NOCOMMIT = (1 << 2),
	//CLI_FLAG_BATCH =    (1 << 3),
	//CLI_FLAG_SHOW_EXT = (1 << 4),
//} flags;

struct uci_type_list {
	unsigned int idx;
	const char *name;
	struct uci_type_list *next;
};

//static struct uci_type_list *type_list = NULL;
//static char *typestr = NULL;
//static const char *cur_section_ref = NULL;

void uci2_reset_typelist(){
}
/*
static void
uci_reset_typelist(struct uci_type_list *type_list)
{
	struct uci_type_list *type;
	while (type_list != NULL) {
			type = type_list;
			type_list = type_list->next;
			free(type);
	}
	if (typestr) {
		free(typestr);
		typestr = NULL;
	}
	cur_section_ref = NULL;
}*/


static void uci_show_value(struct uci_option *o, char value[]){
	struct uci_element *e;
	bool sep = false;
//	char *space;

	switch(o->type) {
	case UCI_TYPE_STRING:
		sprintf(value,"%s", o->v.string);
		break;
	case UCI_TYPE_LIST:
		uci_foreach_element(&o->v.list, e) {
			sprintf(value,"%s", (sep ? delimiter : ""));
			//space = strpbrk(e->name, " \t\r\n");
			//if (!space )
				sprintf(value,"%s", e->name);
			//sep = true;
		}
		break;
	default:
		strcpy(value,"");
		break;
	}
}

struct uci_context* guci2_init(){
	
	struct uci_context* ctx = uci_alloc_context();
		
	return ctx;
}
int guci2_free(struct uci_context* ctx){
	uci_free_context(ctx);
	return 0;
}

int guci2_get(struct uci_context* ctx, const char* section_or_key, char value[]){
	struct uci_ptr ptr;
	struct uci_element *e;
	int ret = UCI_OK;
	char *str=(char*)malloc(strlen(section_or_key)+1); //must not use a const value
	strcpy(str,section_or_key);
	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		ret=-1;
		strcpy(value,"");
		goto out;
	}
	if (!(ptr.flags & UCI_LOOKUP_COMPLETE)) {
		ctx->err = UCI_ERR_NOTFOUND;
		ret=-1;
		strcpy(value,"");
		goto out;
	}
	e = ptr.last;
	switch(e->type) {
	case UCI_TYPE_SECTION:
		sprintf(value,"%s", ptr.s->type);
		break;
	case UCI_TYPE_OPTION:
		uci_show_value(ptr.o, value);
		break;
	default:
		strcpy(value,"");
		ret=-1;
		goto out;
		break;
	}
out:
	free(str);
	return ret;
}


/**
 * guci2_get_idx("wireless.@wifi-iface",0,ssid, value)
 */
int guci2_get_idx(struct uci_context* ctx,const char* section, int index, const char* key, char value[]){
	char s[100]={0};
	if(key!=NULL)
		sprintf(s,"%s[%d].%s",section,index,key);
	else
		sprintf(s,"%s[%d]",section,index);
	return guci2_get(ctx, s,value);
}

int guci2_get_name(struct uci_context* ctx,const char* section, const char* name, const char* key, char value[]){
	char s[100]={0};
	if(key!=NULL)
		sprintf(s,"%s.%s.%s",section,name,key);
	else
		sprintf(s,"%s.%s",section,name);
	return guci2_get(ctx, s,value);
}

int guci2_set(struct uci_context* ctx,const char* section_or_key, const char* value){

	if(value==NULL) return -1;
	//struct uci_element *e;
	struct uci_ptr ptr;
	int ret = UCI_OK;
	char *str=(char*)malloc(strlen(section_or_key)+strlen(value)+3); //must not use a const value

	//FIXME: deal with ' in value
	sprintf(str,"%s=%s",section_or_key,value);
	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		ret=-1;
		goto out;
	}

	ret = uci_set(ctx, &ptr);
		/* save changes, but don't commit them yet */
	if (ret == UCI_OK)
		ret = uci_save(ctx, ptr.p);

out:
	free(str);
	return ret;
}

/**
 * guci2_set_idx("wireless.@wifi-iface", 0, "ssid", "something");
 */
int guci2_set_idx(struct uci_context* ctx,const char* section, int index, const char* key, char* value){
	char s[100]={0};
	if(key !=NULL)
		sprintf(s,"%s[%d].%s",section,index,key);
	else
		sprintf(s,"%s[%d]",section,index);
	return guci2_set(ctx,s,value);
}
int guci2_set_name(struct uci_context* ctx,const char* section, const char* name, const char* key, const char* value){
	char s[100]={0};
	if(key !=NULL)
		sprintf(s,"%s.%s.%s",section,name,key);
	else
		sprintf(s,"%s.%s",section,name);
	return guci2_set(ctx,s,value);
}
int guci2_commit(struct uci_context* ctx,const char* config){
	int ret = 1;

	//struct uci_element *e = NULL;
	struct uci_ptr ptr;
	char *str=(char*)malloc(strlen(config)+1); //must not use a const value
	strcpy(str,config);
	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		free(str);
		sync();
		return 1;
	}
	//e = ptr.last;
	if (uci_commit(ctx, &ptr.p, false) != UCI_OK) {
		goto out;
	}
	ret = 0;
out:
	free(str);
	sync();
	return ret;
}

int guci2_add_list(struct uci_context* ctx,char* key, char* value){
	int ret=-1;
	struct uci_ptr ptr;
	char *str=(char*)malloc(strlen(key)+strlen(value)+1);
	sprintf(str,"%s=%s",key,value);
	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		ret=-1;
		goto out;
	}
	ret = uci_add_list(ctx, &ptr); //return 0 success

out:
	free(str);
	return ret;
}

int guci2_delete(struct uci_context* ctx,const char* key){
	//struct uci_element *e;
	struct uci_ptr ptr;
	int ret = 0;
	char *str=(char*)malloc(strlen(key)+1); //must not use a const value
	strcpy(str,key);
	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		ret=-1;
		goto out;
	}
	ret = uci_delete(ctx, &ptr);
	/* save changes, but don't commit them yet */
	if (ret == UCI_OK)
		ret = uci_save(ctx, ptr.p);

out:
	free(str);
	return ret;
}

int guci2_delete_list_value(struct uci_context* ctx,char* key, char* value){
	int ret=-1;
	struct uci_ptr ptr;
	char *str=(char*)malloc(strlen(key)+strlen(value)+1);
	sprintf(str,"%s=%s",key,value);
	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		ret=-1;
		goto out;
	}
	ret = uci_del_list(ctx, &ptr); //return 0 success

out:
	free(str);
	return ret;
}
extern int guci2_delete_list(struct uci_context* ctx,char* key);

int guci2_add(struct uci_context* ctx,const char* section, const char* type){
	return guci2_set(ctx,section,type);
}

int guci2_add_anonymous(const char *config, const char *session)
{	
	int ret = 0;

	struct uci_context *ctx = NULL;
	struct uci_package *pkg = NULL;
	
	ctx = uci_alloc_context();
	if (!ctx) {
		return -1;
	}

	ret = uci_load(ctx, config, &pkg);
	if (ret != UCI_OK) {
		goto out;
	}
	
	pkg = uci_lookup_package(ctx, config);
	if (pkg) {
		struct uci_ptr ptr = {
			.p = pkg
		};
		
		uci_add_section(ctx, pkg, session, &ptr.s);
	
		uci_commit(ctx, &ptr.p, false);
		uci_unload(ctx, ptr.p);
	} else {
		ret = -1;
	}

out:
	if (ctx) {
		uci_free_context(ctx);
	}

	return ret;
}


/**
 * guci_section_count("wireless.@wifi-iface")
 */
int guci2_section_count(struct uci_context* ctx,const char* section_type){
	struct uci_element *e;
	struct uci_ptr ptr;
	int ret = 0;
	//int len=strlen(section_type);
	char str[100]={0}, str1[100]={0};
	//char *str1=(char*)malloc(sizeof(char)*(len+1)); //must not use a const value

	strcpy(str1,section_type);

	char *end;
	char *section=strtok_r(str1,".@",&end);
	char *type=strtok_r(NULL,".@",&end);
	//char *str=(char*)malloc(sizeof(char)*(len+1)); //must not use a const value
	strcpy(str,section);

	struct uci_package *p=NULL;

	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		ret=0;
		goto out;
	}

	uci2_reset_typelist();
	p=ptr.p;

	uci_foreach_element( &p->sections, e) {
		struct uci_section *s = uci_to_section(e);
		if(strcmp(type, s->type)==0) ret++;
	}
	uci2_reset_typelist();

out:
	//free(str);
	//free(str1);
	return ret;
}

/**
 * get the section name by index
 * guci_section_name("wireless.@wifi-iface",0)
 * @return section name, e.g. "public"
 */
char* guci2_section_name(struct uci_context* ctx,const char* section_type, int index){
	char full_type[100]={0};
	sprintf(full_type,"%s[%d]",section_type,index);

	//char *str1=malloc(strlen(section_type)+1); //must not use a const value
	//strcpy(str1,section_type);
	//char *section=strtok(str1,".@");
	//char *type=strtok(NULL,".@");

	//struct uci_element *e;
	struct uci_ptr ptr;
	struct uci_section *s=NULL;
	char *name=NULL;
	//char *str=malloc(strlen(section_type)+1); //must not use a const value
	//strcpy(str,full_type);
	if (uci_lookup_ptr(ctx, &ptr, full_type, true) != UCI_OK) {
		goto out;
	}
	//uci_reset_typelist();
	//struct uci_package *p=ptr.p;
	s=ptr.s;
	name=s->e.name;
	//int i=0;
	//uci_foreach_element( &p->sections, e) {
		//if(i==index){
			//struct uci_section *s = uci_to_section(e);
			//name=s->e.name;
		//}
		//i++;
	//}
	//uci_reset_typelist();

out:
	//free(str);

	return name;
}
/**
 * guci_find_section("firewall.@zone.name","wan")
 */
char* guci2_find_section(struct uci_context* ctx,const char* section_key, char* value){
	struct uci_element *e;
	struct uci_element *e1;
	struct uci_ptr ptr;
	char *ret = NULL;
	//int len=strlen(section_type);
	char str[100]={0}, str1[100]={0};
	//char *str1=(char*)malloc(sizeof(char)*(len+1)); //must not use a const value

	strcpy(str1,section_key);

	char *end;
	char *section=strtok_r(str1,".@",&end);
	char *type=strtok_r(NULL,".@",&end);
	char *key=strtok_r(NULL,".",&end);
	//char *str=(char*)malloc(sizeof(char)*(len+1)); //must not use a const value
	strcpy(str,section);

	struct uci_package *p=NULL;

	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		ret=NULL;
		goto out;
	}

	uci2_reset_typelist();
	p=ptr.p;

	//printf("looking for section %s\n",section);
	//printf("looking for type %s\n",type);
	//printf("looking for key %s\n",key);
	//printf("looking for value %s\n",value);
	
	uci_foreach_element( &p->sections, e) {
		//printf("checking config name %s\n",e->name);
		struct uci_section *s = uci_to_section(e);
		//printf("checking type %s\n",s->type);
		if(strcmp(type, s->type)==0){		
			//fixme: this maybe wrong because of typelist
			uci_foreach_element( &s->options, e1){		
				if(strcmp(key,e1->name)==0){
					char key_value[128]={0};
					struct uci_option *o = uci_to_option(e1);
					uci_show_value(o,key_value);
					//printf("checking value %s\n",key_value);
					if(strcmp(key_value,value)==0) {
						ret=e->name;
						goto out;
					}
				}
			}
		}
	}
	uci2_reset_typelist();

out:
	//free(str);
	//free(str1);
	return ret;
}

 char *guci2_find_list_member(struct uci_context* ctx,const char* section_key, char* value)
 {
	 char *ret=NULL;
 	struct uci_ptr ptr;
	struct uci_element *e;
	char tmp_v[200]={0};
	char *str=(char*)malloc(strlen(section_key)+1);
	memcpy(str,section_key,strlen(section_key)+1);
	if(UCI_OK==guci2_get(ctx,str,tmp_v)){
		if(UCI_OK==uci_lookup_ptr(ctx,&ptr,str,true)){
			struct uci_option *o=ptr.o;
			switch(o->type){
			case UCI_TYPE_STRING:
				break;
			case UCI_TYPE_LIST:
				uci_foreach_element(&o->v.list, e) {
					if(strcmp(e->name,value)==0){
						ret = e->name;
						goto out;
					}
				}
				break;
			}
		}
	}
out:
	free(str);
	return ret;	
}

char *guci2_get_list_index(struct uci_context* ctx,const char* section_key, int index,char *buf)
 {
	char *ret=NULL;
	struct uci_ptr ptr;
	struct uci_element *e;
	char tmp_v[200]={0};
	int i=0;
	char *str=(char*)malloc(strlen(section_key)+1);
	memcpy(str,section_key,strlen(section_key)+1);
	if(UCI_OK==guci2_get(ctx,str,tmp_v)){
		if(UCI_OK==uci_lookup_ptr(ctx,&ptr,str,true)){
			struct uci_option *o=ptr.o;
			switch(o->type){
			case UCI_TYPE_STRING:
				break;
			case UCI_TYPE_LIST:
				uci_foreach_element(&o->v.list, e) {
					if(i == index){
						strcpy(buf,e->name);
						goto out;
					}
					i++;
				}
				break;
			}
		}
	}
out:
	free(str);
	return ret;
}

///**
// * Test
// *
//int main(int argc, char** argv){
//	char ret[20]={0};
//	guci2_init();
//	char* command=argv[1];
//
//	if(strcmp(command,"get")==0){
//		guci2_get(argv[2], ret);
//		printf("%s\n",ret);
//	}else if(strcmp(command,"set")==0){
//		guci2_set(argv[2],argv[3]);
//	}else if(strcmp(command,"commit")==0){
//		guci2_commit(argv[2]);
//	}else if(strcmp(command,"delete")==0){
//		guci2_delete(argv[2]);
//	}else if(strcmp(command,"connect2wifi")==0){
//		connect2wifi(argv[2],argv[3],argv[4],atoi(argv[5]));
//	}else if(strcmp(command,"connect2dhcp")==0){
//		connect2dhcp();
//	}else if(strcmp(command,"count")==0){
//		int count=guci2_section_count(argv[2]);
//		printf("%d\n",count);
//	}else if(strcmp(command,"name")==0){
//		char *name=guci2_section_name(argv[2],atoi(argv[3]));
//		if(name!=NULL)
//			printf("%s\n",name);
//		else
//			printf("<not found>\n");
//	}
//
//	guci2_free();
//	return 0;
//}
//*/
