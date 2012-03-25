#ifndef DNS_H
#define DNS_H

// Define DNS query types
#define DNS_RES_REC_A     1   // A record
#define DNS_RES_REC_NS    2   // NS record
#define DNS_RES_REC_CNAME 5   // CNAME record
#define DNS_RES_REC_SOA   6   // SOA record
#define DNS_RES_REC_PTR   12  // PTR record
#define DNS_RES_REC_MX    15  // MX record


//DNS header structure
struct DNS_HEADER
{
	unsigned short id;        // identification number

	unsigned char rd:1;      // recursion desired
	unsigned char tc:1;      // truncated message
	unsigned char aa:1;      // authoritive answer
	unsigned char opcode:4;  // purpose of message
	unsigned char qr:1;      // query/response flag

	unsigned char rcode:4;   // response code
	unsigned char cd:1;      // checking disabled
	unsigned char ad:1;      // authenticated data
	unsigned char z:1;       // its z! reserved
	unsigned char ra:1;      // recursion available

	unsigned short q_count;     // number of question entries
	unsigned short ans_count;   // number of answer entries
	unsigned short auth_count;  // number of authority entries
	unsigned short add_count;   // number of resource entries
};

//Constant sized fields of query structure
struct QUESTION
{
	unsigned short qtype;
	unsigned short qclass;
};

//Constant sized fields of the resource record structure
#pragma pack(push, 1)
struct R_DATA
{
	unsigned short type;
	unsigned short _class;
	unsigned int ttl;
	unsigned short data_len;
};
#pragma pack(pop)

//Pointers to resource record contents
struct RES_RECORD
{
	unsigned char *name;
	struct R_DATA *resource;
	unsigned char *rdata;
};

//Structure of a Query
typedef struct
{
	unsigned char *name;
	struct QUESTION *ques;
} QUERY;

//void dns_get_a(char *buffer, char **ip_addr);
void change_to_dns_name_format(unsigned char* dns, unsigned char* host);
void dns_query_a_record(char *server, char *host, char *ip_addr[]);
void dns_query_ptr_record(char *server, char *ip, char *domains[]);
void prep_inaddr_arpa(char *dest, char *src);
unsigned char* read_name(unsigned char *reader, unsigned char *buffer, int *count);
//void dns_print_packet_info(char *buffer);

#endif /* DNS_H */
