#include <iostream>
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
#include <event.h>
#include <json/json.h>
using namespace std;

typedef enum _MsgType
{
    EN_MSG_LOGIN=1,
    EN_MSG_REGISTER,
    EN_MSG_CHAT,
    EN_MSG_OFFLINE,
    EN_MSG_ACK
}EnMsgType;

//全局记录用户名 密码  电话
char name[20];
char pwd[20];
char phone[11];
//异步接收读线程
void* ReadThread(void *arg)
{
    int clientfd = *(int*)arg;
    char recvbuf[1024];
    Json::Reader reader;
    Json::Value root;
    int size=0; 
    while(true)
    {
        size = recv(clientfd, recvbuf, 1024, 0);
        if(size < 0)
        {
            cout<<"server connect fail!"<<endl;
            break;
        }
        if(reader.parse(recvbuf, root))
        {
            int msgtype = root["msgtype"].asInt();
            switch(msgtype)
            {
                case EN_MSG_CHAT:
                {
	                cout<<"recieve chat message:"<<endl;
                    cout<<root["from"].asString()<<":"<<root["ackcode"].asString()<<endl;				
                    cout<<root.toStyledString().c_str()<<endl;
                }
                break;
            }
        }
    }
}
    
bool doLogin(int fd)
{
    cout<<"name:";
    cin.getline(name, 20);
    cout<<"pwd:";
    cin.getline(pwd, 20);
    
    //组装json字符串
    Json::Value root;
    root["msgtype"] = EN_MSG_LOGIN;
    root["name"] = name;
    root["pwd"] = pwd;
    
    cout<<"json:"<<root.toStyledString()<<endl;
    //发送登录消息到server
    int size = send(fd, root.toStyledString().c_str(),
        strlen(root.toStyledString().c_str())+1, 0);
    if(size < 0)
    {
        cout<<"send login msg fail!"<<endl;
        exit(0);
    }
    
    //接收server返回的登录消息
    char recvbuf[1024]={0};
    size = recv(fd, recvbuf, 1024, 0);
    if(size < 0)
    {
        cout<<"recv server login ack fail!"<<endl;
        exit(0);
    }
    
    Json::Reader reader;
    if(reader.parse(recvbuf, root))
    {
        int msgtype = root["msgtype"].asInt();
        if(msgtype != EN_MSG_ACK)
        {
            cout<<"recv server login ack msg invalid!"<<endl;
            exit(0);
        }
        string ackcode = root["ackcode"].asString();
        cout<<root.toStyledString()<<endl;
        if(ackcode == "ok")
        {
            return true;
        }
        return false;
    }
    return false;
}
void doregister(int fd)
{
	cout<<"name:"<<endl;
	cin.getline(name,20);
	cout<<"pwd:"<<endl;
	cin.getline(pwd,20);
	cout<<"phone:"<<endl;
	cin.getline(phone,11);

	//组装json字符串
	Json::Value root;
    root["msgtype"] = EN_MSG_REGISTER;
    root["name"] = name;
    root["pwd"] = pwd;
    root["phone"] = phone;

    cout<<root.toStyledString()<<endl;
	
    //发送注册消息到server
    int size = send(fd, root.toStyledString().c_str(),
        strlen(root.toStyledString().c_str())+1, 0);
    if(size < 0)
    {
        cout<<"send register msg fail!"<<endl;
        exit(0);
    }

	//接受server返回的注册消息
	
    char recvbuf[1024]={0};
    size = recv(fd, recvbuf, 1024, 0);
    if(size < 0)
    {
        cout<<"recv server register ack fail!"<<endl;
        exit(0);
    }
    
    Json::Reader reader;
    if(reader.parse(recvbuf, root))
    {
        int msgtype = root["msgtype"].asInt();
        if(msgtype != EN_MSG_ACK)
        {
            cout<<"recv server register ack msg invalid!"<<endl;
            exit(0);
        }
        string ackcode = root["ackcode"].asString();
        cout<<root.toStyledString()<<endl;
        if(ackcode == "OK")
        {
            cout<<"register success!"<<endl;
            return ;
        }
        cout<<"register error! the user is already register!"<<endl;
    }
}
// 命令行参数传入服务器port
int main(int argc, char **argv)
{
    if(argc < 2)
    {
        cout<<"input ./client port"<<endl;
        return -1;
    }
    
    int port=0;
    port = atoi(argv[1]);
    int clientfd;
    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(clientfd == -1)
    {
        cout<<"clientfd create fail!"<<endl;
        return -1;
    }
    
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if(-1 == connect(clientfd, (sockaddr*)&server, sizeof(server)))
    {
        cout<<"connect server fail!"<<endl;
        return -1;
    }
   
    int choice = 0;
    bool bloginsuccess = false;
    while(!bloginsuccess)
    {
        cout<<"============"<<endl;
        cout<<"1.login"<<endl;
        cout<<"2.register"<<endl;
        cout<<"3.exit"<<endl;
        cout<<"============"<<endl;
        cout<<"choice:";
        cin>>choice;
        cin.get();
        
        switch(choice)
        {
            case 1:
            //login
	            if(doLogin(clientfd))
	            {
	                bloginsuccess = true;  
	            }
	            else
	            {
	                cout<<"login fail!name or pwd is wrong!"<<endl;
	            }  
            break;
            case 2:
            //register
            	doregister(clientfd);
            continue;
            case 3:
            //exit
            	cout<<"bye bye..."<<endl;
            	//客户端退出后保证服务器正常
            	
            	exit(0);
            break;
            default:
                cout<<"invalid input!"<<endl;
            continue;
        }
    }
    
    cout<<"welcome to chat system!"<<endl;
    //zhang lu cheng:msg sdfasdf
    
    //启动异步接收读线程
    pthread_t tid;
    pthread_create(&tid, NULL, ReadThread, &clientfd);
    
    int size=0;
    while(true)
    {
        char chatbuf[1024] = {0};
        cin.getline(chatbuf, 1024);
        
        if(strcmp(chatbuf, "quit") == 0)
        {
            exit(0);
        }
        
        string parsestr = chatbuf;
        int offset = parsestr.find(':');
        //保证消息格式正确
        if(offset == -1)
        {
	    	cout<<"input msg invalid! please input again:"<<endl;
	    	continue;
        }
        Json::Value root;
        root["msgtype"] = EN_MSG_CHAT;
        root["from"] = name;
        root["to"] = parsestr.substr(0, offset);
        root["msg"] = parsestr.substr(offset+1, parsestr.length()-offset-1);
        
        size = send(clientfd, root.toStyledString().c_str(),
            strlen(root.toStyledString().c_str())+1, 0);
    }

    return 0;
}
