
#include "mlc_conf.h"
#include <string.h>
#include "core/log.h"
#include "core/error_code.h"

static const char *url_prefix = "mlc://";

int str_start_with(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? -1 : strncmp(pre, str, lenpre) == 0;
}

int mlc_url_to_conf(const char *url,struct mlc_addr_conf_s *conf)
{
	int ret = str_start_with(url_prefix,url);
	if (ret < 0)
	{
		log_info(NULL,"url %s not start with %s",url,url_prefix);
		return ret;
	}
	const char *host = strstr(url,url_prefix);
	if (host == NULL)
	{
		log_info(NULL,"url host cannot find %s",url);
		return MLC_URL_ERROR;
	}
	host += strlen(url_prefix);
	const char *ip = strstr(host,":");
	if (ip == NULL)
	{
		/* code */
		log_info(NULL,"url ip cannot find %s",url);
		return MLC_URL_ERROR;
	}
	int hostlen = ip - host;
	ip += 1;
	memcpy(conf->ip,host,hostlen);
	conf->ip[hostlen] = 0;
	conf->port = atoi(ip);
	return 0;
}