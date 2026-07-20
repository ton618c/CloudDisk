#include "auth.srpc.h"
#include "workflow/WFFacilities.h"

using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}

static void authregister_done(RegisterResponse *response, srpc::RPCContext *context)
{
}

static void authlogin_done(LoginResponse *response, srpc::RPCContext *context)
{
}

int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	const char *ip = "127.0.0.1";
	unsigned short port = 1412;

	Auth::SRPCClient client(ip, port);

	// example for RPC method call
	RegisterRequest authregister_req;
	//authregister_req.set_message("Hello, srpc!");
	client.AuthRegister(&authregister_req, authregister_done);

	wait_group.wait();
	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}
