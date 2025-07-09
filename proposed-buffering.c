int tran_func(thread_param_t* param_p)
{
//*****************************************************************************
	int					      target=1000;									        // The target value [bytes]
  struct  timespec	tran_recv_req={0,125000};							// The control interval [nano sec]
//*****************************************************************************

  // Receiveing Process
	while( param_p->error_flg == 0 ){
		
    // Set the control interval
		nanosleep(&tran_recv_req, NULL);

    // Culculate the difference between the target value and the current amount of data in the NCC buffer 
		gap_datasize = target - (param_p->recv_total - param_p->send_total);

    // Dertermin the amount of data to move in a single recv() function
		if( gap_datasize > 0 ){
			if( gap_datasize >= param_p->buf_size ){
				recv_size = param_p->buf_size;
			}else{
				recv_size = gap_datasize;
			}
		}else{
			continue;
		}
  }
	return 0;
}
