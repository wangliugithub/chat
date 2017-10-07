#include <iostream>
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <event.h>
#include <json/json.h>
#include <errno.h>
using namespace std;

class User
{
public:
    User(){}
    User(string name, string pwd, string call,int fd)
        :_name(name), _pwd(pwd), _call(call),_fd(fd){}
    string getName(){return _name;}
    string getPwd(){return _pwd;}
    string getcall(){return _call;}
    void setfd(int fd){_fd = fd;}
    int getfd(){return _fd;}
private:
    string _name;
    string _pwd;
    string _call;
    int _fd;
};
//模拟数据库里面的人员信息
map<string, User> gUserDBMap;

typedef enum _MsgType
{
    EN_MSG_LOGIN=1,
    EN_MSG_REGISTER,
    EN_MSG_CHAT,
    EN_MSG_OFFLINE,
    EN_MSG_ACK
}EnMsgType;


//异步接收读线程
void* ReadThread(void *arg)
{
    int clientfd = (int)arg;
    while(true)
    {
        //接收client发送的消息，处理，响应
        //当接收到quit时,或程序异常退出
        
        char recvbuf[1024]={0};
        int size = 0;
        Json::Reader reader;
        Json::Value root;
        Json::Value response;
        
        size = recv(clientfd, recvbuf, 1024, 0);
        if(size < 0)
        {
            cout<<"errno:"<<errno<<endl;
            cout<<"client connect fail!"<<endl;
            close(clientfd);
            return NULL;
        }
    
        cout<<"recvbuf:"<<recvbuf<<endl;
        
        //解析json消息
        if(reader.parse(recvbuf, root))
        {
            int msgtype = root["msgtype"].asInt();
            switch(msgtype)
            {
                case EN_MSG_LOGIN:
                {
                    response["msgtype"] = EN_MSG_ACK;
                    string name = root["name"].asString();
                    string pwd = root["pwd"].asString();

                    map<string, User>::iterator it = gUserDBMap.find(name);
                    if(it == gUserDBMap.end())
                    {
                        response["ackcode"] = "error";
                    }
                    else
                    {
	                    //登录名 密码匹配成功 记录
                        if(pwd == it->second.getPwd())
                        {
			                it->second.setfd(clientfd);//标记上线时fd
                            response["ackcode"] = "ok"; 
                            cout<<clientfd<<endl;  
                        }
                        else
                        {
                            response["ackcode"] = "error";
                        }
                    }
                
                    //发送响应
                    cout<<"response:"<<response.toStyledString()<<endl;
                    send(clientfd, response.toStyledString().c_str(),
                        strlen(response.toStyledString().c_str())+1, 0);
                        //response.toStyledString() JSON对象序列化成字符串
                        //c_str()函数返回一个指向正规C字符串的指针常量
                }
                break;
                case EN_MSG_CHAT:
                {
	                
                    cout<<root["from"]<<" => "<<root["to"]<<":"<<root["msg"]<<endl;
                    string to = root["to"].asString();
                    string from = root["from"].asString();                   
                    string msg = root["msg"].asString();  
                    //如果此人没有登陆提示
                    map<string,User>::iterator it = gUserDBMap.find(to);
                    
                    if(it == gUserDBMap.end())
                    {
	                    string str = "can not fint this people! please input again:";
	                    cout<<str<<endl;
	                    response["msgtype"] = EN_MSG_CHAT;                         	
                    	response["ackcode"] = str;
                    	if(-1 == send(clientfd,response.toStyledString().c_str(),strlen(response.toStyledString().c_str())+1,0))
                        {
	                    	cout<<"server send message error"<<endl;
                    	}
                    }
                    if(it->second.getfd() == -1)
                    {
	                    string str = to + "is not logining please wait:";
	                	cout<<str<<endl; 
	                	response["msgtype"] = EN_MSG_CHAT;                	
                    	response["ackcode"] = str;
                    	if(-1 == send(clientfd,response.toStyledString().c_str(),strlen(response.toStyledString().c_str())+1,0))
                        {
	                    	cout<<"server send message error"<<endl;
                    	}
                    }
                    
                    else
                    {
	                    response["msgtype"] = EN_MSG_CHAT;            
                    	response["from"] = from;
                    	response["ackcode"] = msg;
                    	if(-1 == send(gUserDBMap[to].getfd(),response.toStyledString().c_str(),strlen(response.toStyledString().c_str())+1,0))
                        {
	                    	cout<<"server send message error"<<endl;
                    	}
                    }		    	
                    
                }
                break;
                
                case EN_MSG_REGISTER:
	            {
		            cout<<"one client is register"<<endl;
		            string name = root["name"].asString();
                    string pwd = root["pwd"].asString();
                    string phone = root["phone"].asString();
                    
		        	response["msgtype"] = EN_MSG_ACK;
					//判断用户是否已注册
					
					map<string, User>::iterator it = gUserDBMap.find(name);
                    if( (it != gUserDBMap.end()) && (it->second.getcall() == phone))
                    {
                        
		                cout<<"the user is already register"<<endl;
		                response["ackcode"] = "Error"; 
	                    
                    }
                 	else
	                {
		                //注册的用户信息插入到数据库中并保存		        	
                    	gUserDBMap[name] = User(name,pwd,phone,-1);                    						response["ackcode"] = "OK";
	                }	                	                  		        		        	
		        	if(-1 == send(clientfd,response.toStyledString().c_str(),strlen(response.toStyledString().c_str())+1,0))
                    {
	                    cout<<"server send register message error"<<endl;
                    }
                                     
	            }
                break;
            }
        }
        //客户端异常结束处理
        else
        {
	        cout<<"one client close!"<<endl;
            close(clientfd);
        }
    }
}

//libevent回调函数
void ProcListenfd(evutil_socket_t fd, short , void *arg)
{
    sockaddr_in client;
    socklen_t len = sizeof(client);
    int clientfd = accept(fd, (sockaddr*)&client, &len);
  
    cout<<"new client connect server! client info:"
        <<inet_ntoa(client.sin_addr)<<" "<<ntohs(client.sin_port)<<endl;
        
    //启动异步接收读线程   一个客户端  《=》   线程
    pthread_t tid;
    pthread_create(&tid, NULL, ReadThread, (void*)clientfd);
}

int main()
{
    int listenfd;
    //模拟给map添加人员信息
    gUserDBMap["zhang san"] = User("zhang san", "111111", "9870986796",-1);
    gUserDBMap["xiao ming"] = User("xiao ming", "222222", "7777777",-1);
    gUserDBMap["gao yang"] = User("gao yang", "333333", "66666666",-1);
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1)
    {
        cout<<"listenfd create fail!"<<endl;
        return -1;
    }
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(10000);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if(-1 == bind(listenfd, (sockaddr*)&server, sizeof(server)))
    {
        cout<<"listenfd bind fail!"<<endl;
        return -1;
    }
    
    if(-1 == listen(listenfd, 20))
    {
        cout<<"listenfd listen fail!"<<endl;
        return -1;
    }
    
    //创建reactor   统一事件源  socket I/O，信号，定时器
    struct event_base* base = event_init();
    //创建event事件
    struct event *listen_event = event_new(base, listenfd,  EV_READ|EV_PERSIST, ProcListenfd, NULL);
    //把event事件添加到reactor中
    event_add( listen_event, NULL );
    
    cout<<"server started..."<<endl;
    //启动反应堆
    event_base_dispatch(base);
    //释放事件资源
    event_free(listen_event);
    //关闭reactor
    event_base_free(base);
    
    return 0;
}
