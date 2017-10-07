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
//ģ�����ݿ��������Ա��Ϣ
map<string, User> gUserDBMap;

typedef enum _MsgType
{
    EN_MSG_LOGIN=1,
    EN_MSG_REGISTER,
    EN_MSG_CHAT,
    EN_MSG_OFFLINE,
    EN_MSG_ACK
}EnMsgType;


//�첽���ն��߳�
void* ReadThread(void *arg)
{
    int clientfd = (int)arg;
    while(true)
    {
        //����client���͵���Ϣ��������Ӧ
        //�����յ�quitʱ,������쳣�˳�
        
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
        
        //����json��Ϣ
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
	                    //��¼�� ����ƥ��ɹ� ��¼
                        if(pwd == it->second.getPwd())
                        {
			                it->second.setfd(clientfd);//�������ʱfd
                            response["ackcode"] = "ok"; 
                            cout<<clientfd<<endl;  
                        }
                        else
                        {
                            response["ackcode"] = "error";
                        }
                    }
                
                    //������Ӧ
                    cout<<"response:"<<response.toStyledString()<<endl;
                    send(clientfd, response.toStyledString().c_str(),
                        strlen(response.toStyledString().c_str())+1, 0);
                        //response.toStyledString() JSON�������л����ַ���
                        //c_str()��������һ��ָ������C�ַ�����ָ�볣��
                }
                break;
                case EN_MSG_CHAT:
                {
	                
                    cout<<root["from"]<<" => "<<root["to"]<<":"<<root["msg"]<<endl;
                    string to = root["to"].asString();
                    string from = root["from"].asString();                   
                    string msg = root["msg"].asString();  
                    //�������û�е�½��ʾ
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
					//�ж��û��Ƿ���ע��
					
					map<string, User>::iterator it = gUserDBMap.find(name);
                    if( (it != gUserDBMap.end()) && (it->second.getcall() == phone))
                    {
                        
		                cout<<"the user is already register"<<endl;
		                response["ackcode"] = "Error"; 
	                    
                    }
                 	else
	                {
		                //ע����û���Ϣ���뵽���ݿ��в�����		        	
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
        //�ͻ����쳣��������
        else
        {
	        cout<<"one client close!"<<endl;
            close(clientfd);
        }
    }
}

//libevent�ص�����
void ProcListenfd(evutil_socket_t fd, short , void *arg)
{
    sockaddr_in client;
    socklen_t len = sizeof(client);
    int clientfd = accept(fd, (sockaddr*)&client, &len);
  
    cout<<"new client connect server! client info:"
        <<inet_ntoa(client.sin_addr)<<" "<<ntohs(client.sin_port)<<endl;
        
    //�����첽���ն��߳�   һ���ͻ���  ��=��   �߳�
    pthread_t tid;
    pthread_create(&tid, NULL, ReadThread, (void*)clientfd);
}

int main()
{
    int listenfd;
    //ģ���map�����Ա��Ϣ
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
    
    //����reactor   ͳһ�¼�Դ  socket I/O���źţ���ʱ��
    struct event_base* base = event_init();
    //����event�¼�
    struct event *listen_event = event_new(base, listenfd,  EV_READ|EV_PERSIST, ProcListenfd, NULL);
    //��event�¼���ӵ�reactor��
    event_add( listen_event, NULL );
    
    cout<<"server started..."<<endl;
    //������Ӧ��
    event_base_dispatch(base);
    //�ͷ��¼���Դ
    event_free(listen_event);
    //�ر�reactor
    event_base_free(base);
    
    return 0;
}
