#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mysql.h>

#include "../rtg.h"
#include "../rtgplot.h"

#define MAX_TABLE_NAME 128
#define MAX_GROUP_NAME 128
#define MAX_COUNTER_NAME 128

#define MAX_TABLE_IDS 32

#define MAX_GROUPS 32
#define MAX_TABLES 32

typedef struct table_struct {

	char name[MAX_TABLE_NAME];

	unsigned int ids[MAX_TABLE_IDS];
	unsigned int ids_count;

	data_t *data[MAX_TABLE_IDS];
	rate_t rate[MAX_TABLE_IDS];

} table_t;

typedef struct group_struct {

	char name[MAX_GROUP_NAME];

	table_t tables[MAX_TABLES];
	//char counter_names[MAX_TABLES][MAX_COUNTER_NAME];

	unsigned int tables_count;

} group_t;

group_t groups[MAX_GROUPS];
unsigned int groups_count;

int parse_group_data(char *str) {

	char *semi_col = NULL;
	char *p = str;
	
	unsigned int group_id = -1;
	unsigned int table_id = 0;
	
	group_t *group;
	
	while ((semi_col = strchr(p, ';')) != NULL) {
		
		char *chunk = (char *) calloc(semi_col - p + 1, sizeof(char));
		strncpy(chunk, p, semi_col - p);
		
		if (strstr(chunk, "gn:") == chunk) { // we got a group name;

			group_id++;
			
			if (group_id > 0) groups[group_id - 1].tables_count = table_id;
			
			table_id = 0;
			
			memset(&groups[group_id], 0, sizeof(group_t));
			group = &groups[group_id];
			
			strcpy(group->name, chunk + 3);
			//printf("gn:%s\n", chunk + 3);

/*
		} else if (strstr(chunk, "cn:") == chunk) { // we got a group name;
		
			table_id++;
			strcpy(group -> counter_names[table_id], chunk + 3);
		
			printf("cn:%s\n", chunk + 3);
			
*/
		} else { // got a table name  + ids;

			unsigned int id_id = 0;

			char *colon = strchr(chunk, ':');
			
			char *tname = (char *) calloc(colon - chunk, sizeof(char));

			strncpy(groups[group_id].tables[table_id].name, chunk, colon - chunk);

			//strncpy(tname, chunk, colon - chunk);
			//printf("tn:%s\n", tname);
			
			char *comma = strchr(colon, ',');
			char *id = (char *) calloc(16, sizeof(char));
			
			if (comma == NULL) { // only one id;

				strcpy(id, colon + 1);
				(group->tables[table_id]).ids[id_id] = atoi(id);
				id_id++;
				//printf("\tid:%s\n", id);
				
			} else {

				do {

					memset(id, 0, 16);
					strncpy(id, colon + 1, comma - (colon + 1));
					
					(group->tables[table_id]).ids[id_id] = atoi(id);
					id_id++;
					
					// printf("\tid:%s\n", id);
					colon = comma;
					
				} while ((comma = strchr(colon + 1, ',')) != NULL);
				
				memset(id, 0, 16);
				strcpy(id, colon + 1);

				(group->tables[table_id]).ids[id_id] = atoi(id);
				id_id++;
				
				//printf("\tid:%s\n", id);
				
			}
			
			(group->tables[table_id]).ids_count = id_id;
			
			table_id++;
			
			free(tname);
			free(id);

		}

		p = semi_col + 1;
		
		free(chunk);

	}
	
	groups[group_id].tables_count = table_id;
	
	groups_count = group_id + 1;
}

/* int parse_table_data(char *line) {

	char *plus = strchr(line, '+');

	int group_id = 0;
	
	memset(&groups[group_id], 0, sizeof(group_t));
	
	if (plus == NULL) { // only one id;
	
		//sprintf(groups[group_id].name, "%d", group_id);
		parse_group_data(line, &groups[group_id]);
		
	} else {
	  
		char *str = (char*) calloc(512, sizeof(char));
		do {
		  
			memset(str, 0, 512);
			strncpy(str, line, plus - line);
			line = plus + 1;
			//sprintf(groups[group_id].name, "%d", group_id);
			parse_group_data(str, &groups[group_id]);

			group_id++;
			
			memset(&groups[group_id], 0, sizeof(group_t));
			
		} while ((plus = strchr(line, '+')) != NULL);
		
		//sprintf(groups[group_id].name, "%d", group_id);
		parse_group_data(line, &groups[group_id]);
		
	}
	
	groups_count = group_id;
	
	free(plus);
}
*/

int main(int argc, char *argv[]) {

	//char line[] = "gn:input;t1:1;t2:2,23;gn:gogo;t2:2;t3:122,23;gn:pesho;t3:122,23;";
	//char line[] = "gn:some_name;cn:abc;t1:2;t2:122,23;cn:gogo;t2:2;t3:122,23;cn:pesho;t3:122,23;+cn:abc;t1:2;t2:122,23;cn:gogo;t2:2;t3:122,23;";
	
	//char line[] = "gn:Input;ntt_Out_633:9176;romtelecom_Out_640:9231,9261,15067;spnet_Out_646:9301;telia_Out_645:9296,14762;gn:Output;ntt_Out_633:9176;romtelecom_Out_640:9231,9261,15067;spnet_Out_646:9301;telia_Out_645:9296,14762;";
	//http://wm.nat.bg/cgi/rtgplot.cgi?gi=gn:In;biom_In_785:13350;btc_In_725:12649;cabletel_In_711:12789;core_lan_In_216:12594;digsys_In_771:12604;dir_bg_In_764:12399;gcn_In_770:12559;ibgc_In_766:12664;itdnet_In_762:12584;megalan_In_763:12624;mtel_In_378:12494;neterra_In_730:12474;online_direct_In_740:12759;powernet_In_773:12654;spnet_In_646:12509;teraoptics_In_744:12699;tpn_In_767:12524;unisofia_In_775:12719;unix_solutions_In_765:12459;gn:Out;biom_Out_785:13351;btc_Out_725:12650;cabletel_Out_711:12790;core_lan_Out_216:12595;digsys_Out_771:12605;dir_bg_Out_764:12400;gcn_Out_770:12560;ibgc_Out_766:12665;itdnet_Out_762:12585;megalan_Out_763:12625;mtel_Out_378:12495;neterra_Out_730:12475;online_direct_Out_740:12760;powernet_Out_773:12655;spnet_Out_646:12510;teraoptics_Out_744:12700;tpn_Out_767:12525;unisofia_Out_775:12720;unix_solutions_Out_765:12460;&factor=8&begin=1271601660&end=1271688060&units=bits/s&xplot=500&yplot=320&borderb=340&gn=INTERNATIONAL-AGGREGATE-IN/OUT&dm=4&aggr=1
	char line[] = "gn:In;biom_In_785:13350;btc_In_725:12649;cabletel_In_711:12789;core_lan_In_216:12594;digsys_In_771:12604;dir_bg_In_764:12399;gcn_In_770:12559;ibgc_In_766:12664;itdnet_In_762:12584;megalan_In_763:12624;mtel_In_378:12494;neterra_In_730:12474;online_direct_In_740:12759;powernet_In_773:12654;spnet_In_646:12509;teraoptics_In_744:12699;tpn_In_767:12524;unisofia_In_775:12719;unix_solutions_In_765:12459;gn:Out;biom_Out_785:13351;btc_Out_725:12650;cabletel_Out_711:12790;core_lan_Out_216:12595;digsys_Out_771:12605;dir_bg_Out_764:12400;gcn_Out_770:12560;ibgc_Out_766:12665;itdnet_Out_762:12585;megalan_Out_763:12625;mtel_Out_378:12495;neterra_Out_730:12475;online_direct_Out_740:12760;powernet_Out_773:12655;spnet_Out_646:12510;teraoptics_Out_744:12700;tpn_Out_767:12525;unisofia_Out_775:12720;unix_solutions_Out_765:12460;";
	printf("%s\n", line);
	groups_count = 0;
	parse_group_data(line);
	
	int i, j, k;
	
	for (i = 0; i < groups_count; i++) {
		printf("group: %s\n", groups[i].name);
		for (j = 0; j < groups[i].tables_count; j++) {
			printf("\ttable: %s\n", groups[i].tables[j].name);
			for (k = 0; k < groups[i].tables[j].ids_count; k++) {
				printf("\t\tid: %d\n", groups[i].tables[j].ids[k]);
			}
		}
	}
/*	printf("%s\n", groups[2].name);
	printf("%d\n", groups[2].tables_count);
	printf("%d\n", groups[2].tables[0].ids_count);
	printf("%d\n\n", groups[1].tables[0].ids[0]);
	//printf("%d\n", groups[0].tables[1].ids_count);
	
	printf("%s\n", groups[1].tables[0].name);
	printf("%s\n", groups[1].tables[1].name);*/
	
	return 0;
}