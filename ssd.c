/*****************************************************************************************************************************
This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

FileName：ssd.c
Author: Hu Yang      Version: 2.1   Date:2011/12/02
Description: 

History:
<contributor>     <time>        <version>       <desc>                   <e-mail>
Yang Hu         2009/09/25        1.0           Creat SSDsim         yanghu@foxmail.com
                2010/05/01        2.x           Change 
Zhiming Zhu     2011/07/01        2.0           Change               812839842@qq.com
Shuangwu Zhang  2011/11/01        2.1           Change               820876427@qq.com
Chao Ren        2011/07/01        2.0           Change               529517386@qq.com
Hao Luo         2011/01/01        2.0           Change               luohao135680@gmail.com
*****************************************************************************************************************************/
#include "ssd.h"
#include "inttypes.h"

#include "assert.h"
/********************************************************************************************************************************
1. main函数中initiation()函数用来初始化ssd 2.make_aged()函数使SSD成为aged，aged的ssd相当于使用过一段时间的ssd，里面有失效页，
non_aged的ssd是新的ssd，无失效页，失效页的比例可以在初始化参数中设置 3. pre_process_page()函数提前扫一遍读请求，把读请求
的lpn<--->ppn映射关系事先建立好，写请求的lpn<--->ppn映射关系在写的时候再建立，预处理trace防止读请求时读不到数据 4. simulate()是
核心处理函数，trace文件从读进来到处理完成都由这个函数来完成 5. statistic_output()函数将ssd结构中的信息输出到输出文件，输出的是
统计数据和平均数据，输出文件较小，trace_output文件则很大很详细 6. free_all_node()函数释放整个main函数中申请的节点
*********************************************************************************************************************************/
int main(int argc, char **argv)
{
   unsigned  int i,j,k,m;
   struct ssd_info *ssd;
  
   //printf("enter main\n");
   
   ssd=(struct ssd_info*)malloc(sizeof(struct ssd_info));
   alloc_assert(ssd,"ssd");
   memset(ssd,0, sizeof(struct ssd_info));

   if (argc != 2) {
      printf("usage: ssd tracefilename\n");
      exit(1);
   }
   strcpy(ssd->tracefilename, argv[1]);

   ssd=initiation(ssd);
   if (ssd == NULL)
      exit(1);
   
   make_aged(ssd); //set 1/2 threshold pages to be invalid
   pre_set_page(ssd); //fill other 1/2 threshold pages through sequential write
   pre_process_page(ssd);

   /*for (i=0;i<ssd->parameter->channel_number;i++)
      for (j=0;j<ssd->parameter->chip_channel[i];j++)
         for (k=0;k<ssd->parameter->die_chip;k++)
            for (m=0;m<ssd->parameter->plane_die;m++)        
               fprintf(ssd->gc_ratio,"%.3f\n", (double)ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].free_page/(ssd->parameter->page_block*ssd->parameter->block_plane));
   exit(0);//*/
      
   for (i=0;i<ssd->parameter->channel_number;i++)   
      {
         for (j=0;j<ssd->parameter->die_chip;j++)
            {
               for (k=0;k<ssd->parameter->plane_die;k++)
                  {
                     //printf("%d,0,%d,%d:  %5d\n",i,j,k,ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].free_page);
                  }
            }
      }

   
   fprintf(ssd->outputfile,"\t\t\tOUTPUT\n");
   fprintf(ssd->outputfile,"********************* TRACE INFO *********************\n");

   ssd->start_time = -1; 
   ssd=simulate(ssd);
   //printf("read request count: %d, write request count: %d\n", ssd->read_request_count, ssd->write_request_count); 
   statistic_output(ssd);  
   /*free_all_node(ssd);*/
    
   printf("\n");
   printf("the simulation is completed!\n\n\n");
   
   return 1;
}


/******************simulate() *********************************************************************
 *simulate()是核心处理函数，主要实现的功能包括
 *1,从trace文件中获取一条请求，挂到ssd->request
 *2，根据ssd是否有dram分别处理读出来的请求，把这些请求处理成为读写子请求，挂到ssd->channel或者ssd上
 *3，按照事件的先后来处理这些读写子请求。
 *4，输出每条请求的子请求都处理完后的相关信息到outputfile文件中
 **************************************************************************************************/
struct ssd_info *simulate(struct ssd_info *ssd)
{
   int flag=1,flag1=0;
   double output_step=0;
   unsigned int a=0,b=0;
   //errno_t err;

   printf("\n");
   printf("begin simulating.......................\n");
   printf("\n");
   printf("\n");
   printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");

   ssd->tracefile = fopen(ssd->tracefilename,"r");
   if(ssd->tracefile == NULL)
      {  
         printf("the trace file: %s can't open\n", ssd->tracefilename);
         exit(1);
      }

   fprintf(ssd->outputfile,"      arrive           lsn     size ope     begin time    response time    process time\n");   
   fflush(ssd->outputfile);

   int i,j,k,m;
   int c = 0;
   
   while(flag!=100)      
      {
         int i, j;
         /*printf("\nsnapshot of GC:\n");
         for (i=0;i < ssd->parameter->channel_number;i++) 
            for (j=0;j< ssd->parameter->chip_channel[i];j++)
               if (j != ssd->parameter->chip_channel[i] - 1)
                  printf("%d, ", ssd->dram->gc_monitor[i][j].next_state_predict_time?1:0);
               else
                  printf("%d\n", ssd->dram->gc_monitor[i][j].next_state_predict_time?1:0);
         printf("\n");//*/

         //printf("before/get_requests/ssd->current_time: %lld\n", ssd->current_time);      
         flag=get_requests(ssd);
         //printf("flag %d, request queue length: %d\n", flag, ssd->request_queue_length);      
         //printf("after/get_requests/ssd->current_time: %lld\n", ssd->current_time);
         
         for (i=0;i < ssd->parameter->channel_number;i++) 
            for (j=0;j< ssd->parameter->chip_channel[i];j++)
               if (ssd->dram->gc_monitor[i][j].next_state_predict_time <= ssd->current_time && ssd->dram->gc_monitor[i][j].next_state_predict_time > 0)      {                
                  ssd->dram->gc_monitor[i][j].next_state_predict_time = 0;
                  if (ssd->dram->gc_monitor[i][j].complete) {
                     ssd->dram->gc_monitor[i][j].complete = 0;
                     ssd->dram->gc_monitor[i][j].gc_predict_complete_time = 0;
                     ssd->dram->gc_monitor[i][j].gc_interval = ssd->current_time - ssd->dram->gc_monitor[i][j].gc_start_time;                   
                     if (ssd->current_time > ssd->stats_time)
                        ssd->gc_completed++;
                  }      
                  //printf("gc_monitor[%d][%d] = 0\n", i, j);      
               }
      
         if(flag == 1) {          
            //if (ssd->parameter->dram_capacity!=0)
            if (ssd->parameter->buffer_management!=0)
               {
                  if (!(ssd->parameter->raid || ssd->parameter->adaptive_read)) {        
                     buffer_management(ssd);       
                     distribute(ssd);
                  } else {
                     buffer_management_modified(ssd);
                     distribute_modified(ssd);
                  }

                  if (ssd->parameter->buffer_background_flush)
                     buffer_background_flush(ssd);               
               } 
            else
               {
                  no_buffer_distribute(ssd);
               }
         }
    
         process(ssd);
         trace_output(ssd);

         /*c++;
           if (c == 2)
           exit(1);//*/
      
         //printf("flag: %d, queue length: %d, current_time: %lld\n", flag, ssd->request_queue_length, ssd->current_time);
         //if(flag == 0 && ssd->request_queue == NULL) 
         if (flag == 0 && ssd->request_queue_length <= 32) 
            flag = 100;      
      }

   
   /*struct request *req = malloc(sizeof(struct request));
   struct sub_request *sub = malloc(sizeof(struct sub_request));
   req=ssd->request_queue;
   while (req!=NULL) {
      printf("req/operation: %d, lsn: %d, size: %d\n", req->operation, req->lsn, req->size);
      sub=req->subs;
      while (sub!=NULL) {
         printf("sub/operation: %d, lpn: %d, size: %d, parity: %d, current_time: %lld, next_state_predict_time: %lld, current_state: %d, next_state: %d\n", sub->operation, sub->lpn, sub->size, sub->parity, sub->current_time, sub->next_state_predict_time, sub->current_state, sub->next_state);
         sub=sub->next_subs;
      }    
      req=req->next_node;
   }//*/
  
   fclose(ssd->tracefile);
   return ssd;
}


/********    get_request    ******************************************************
 * 1.get requests that arrived already
 * 2.add those request node to ssd->reuqest_queue
 * return   0: reach the end of the trace
 *       -1: no request has been added
 *       1: add one request to list
 *SSD模拟器有三种驱动方式:时钟驱动(精确，太慢) 事件驱动(本程序采用) trace驱动()，
 *两种方式推进事件：channel/chip状态改变、trace文件请求达到。
 *channel/chip状态改变和trace文件请求到达是散布在时间轴上的点，每次从当前状态到达
 *下一个状态都要到达最近的一个状态，每到达一个点执行一次process
 ********************************************************************************/
int get_requests(struct ssd_info *ssd)  
{  
   char buffer[200];
   unsigned int lsn=0;
   int device, size, ope, large_lsn, i = 0, j = 0;
   struct request *request1;
   int flag = 1;
   long filepoint; 
   int64_t time_t = 0;
   int64_t nearest_event_time;    
  

   //printf("enter get_requests, current time:%lld\n",ssd->current_time);
   if(feof(ssd->tracefile))
      return 0;
  
   filepoint = ftell(ssd->tracefile);  
   fgets(buffer, 200, ssd->tracefile); 
   sscanf(buffer,"%" PRId64 " %d %d %d %d",&time_t,&device,&lsn,&size,&ope);

   fprintf(ssd->request_size, "%d\n", size);
   
   if (ssd->start_time == -1)
      ssd->start_time = time_t;
     
   if ((device<0)&&(size<0)&&(ope<0))
      {
         return 100;
      }
   if (lsn<ssd->min_lsn) 
      ssd->min_lsn=lsn;
   if (lsn>ssd->max_lsn)
      ssd->max_lsn=lsn;
   /******************************************************************************************************
    *上层文件系统发送给SSD的任何读写命令包括两个部分（LSN，size） LSN是逻辑扇区号，对于文件系统而言，它所看到的存
    *储空间是一个线性的连续空间。例如，读请求（260，6）表示的是需要读取从扇区号为260的逻辑扇区开始，总共6个扇区。
    *large_lsn: channel下面有多少个subpage，即多少个sector。overprovide系数：SSD中并不是所有的空间都可以给用户使用，
    *比如32G的SSD可能有10%的空间保留下来留作他用，所以乘以1-provide
    ***********************************************************************************************************/
   large_lsn=(int)((ssd->parameter->subpage_page*ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->plane_die*ssd->parameter->die_chip*ssd->parameter->chip_num)*(1-ssd->parameter->overprovide));
   lsn = lsn%large_lsn;

   nearest_event_time=find_nearest_event(ssd);
   if (nearest_event_time==MAX_INT64)
      {
         ssd->current_time=time_t;
         /*if (ssd->request_queue_length>ssd->parameter->queue_length)    //如果请求队列的长度超过了配置文件中所设置的长度                    
            {
               printf("error in get request , the queue length is too long\n");
            }*/
      }
   else
      {   
         if(nearest_event_time<time_t)
            {
               /*******************************************************************************
                *回滚，即如果没有把time_t赋给ssd->current_time，则trace文件已读的一条记录回滚
                *filepoint记录了执行fgets之前的文件指针位置，回滚到文件头+filepoint处
                *int fseek(FILE *stream, long offset, int fromwhere);函数设置文件指针stream的位置。
                *如果执行成功，stream将指向以fromwhere（偏移起始位置：文件头0，当前位置1，文件尾2）为基准，
                *偏移offset（指针偏移量）个字节的位置。如果执行失败(比如offset超过文件自身大小)，则不改变stream指向的位置。
                *文本文件只能采用文件头0的定位方式，本程序中打开文件方式是"r":以只读方式打开文本文件  
                **********************************************************************************/
               //printf("ssd->current_time: %lld, nearest event time: %lld, time_t: %lld\n", ssd->current_time, nearest_event_time, time_t);
               fseek(ssd->tracefile,filepoint,0);
               if(ssd->current_time<=nearest_event_time) 
                  ssd->current_time=nearest_event_time;
               return -1;
            }
         else
            {
               if (ssd->request_queue_length>=ssd->parameter->queue_length)
                  {
                     fseek(ssd->tracefile,filepoint,0);
                     ssd->current_time=nearest_event_time;
                     return -1;
                  } 
               else
                  {
                     /*if (time_t < ssd->current_time) { 
                        printf("nearest_event_time: %lld, time_t: %lld\n", nearest_event_time, time_t);
                        exit(1);
                     } else {
                        printf("time_t: %lld\n", time_t);
                     }*/
                     if (time_t > ssd->current_time) 
                        ssd->current_time=time_t;
                  }
            }
      }

   if(time_t < 0)
      {
         printf("error!\n");
         while(1){}
      }

   if(feof(ssd->tracefile))
      {
         request1=NULL;
         return 0;
      }

   request1 = (struct request*)malloc(sizeof(struct request));
   alloc_assert(request1,"request");
   memset(request1,0, sizeof(struct request));

   request1->time = time_t;
   request1->lsn = lsn;
   request1->size = size;
   request1->operation = ope; 
   request1->begin_time = time_t;
   request1->response_time = 0;  
   request1->energy_consumption = 0;   
   request1->next_node = NULL;
   request1->distri_flag = 0;              //indicate whether this request has been distributed already
   request1->subs = NULL;
   request1->need_distr_flag = NULL;
   request1->complete_lsn_count=0;         //record the count of lsn served by buffer
   //filepoint = ftell(ssd->tracefile);     //set the file point
  
   int first_sn = ssd->parameter->raid ? request1->lsn/(ssd->parameter->subpage_page*(ssd->parameter->channel_number-1)) : request1->lsn/(ssd->parameter->subpage_page*ssd->parameter->channel_number);
   int last_sn = ssd->parameter->raid ? (request1->lsn + request1->size - 1)/(ssd->parameter->subpage_page*(ssd->parameter->channel_number-1)) : (request1->lsn + request1->size - 1)/(ssd->parameter->subpage_page*ssd->parameter->channel_number);  

   request1->gc_count = calloc(last_sn - first_sn + 1, sizeof(int));
   request1->sub_req_dropped = calloc(last_sn - first_sn + 1, sizeof(int));
   if (request1->operation && request1->time < ssd->current_time && request1->time > ssd->stats_time)
      ssd->delayed_requests++;
    
   if(ssd->request_queue == NULL)          //The queue is empty
      {
         ssd->request_queue = request1;
         ssd->request_tail = request1;      
         ssd->request_queue_length++;
      }
   else
      {        
         (ssd->request_tail)->next_node = request1;   
         ssd->request_tail = request1;       
         ssd->request_queue_length++;
      }

   if (request1->operation==1)             //计算平均请求大小 1为读 0为写
      {
         ssd->ave_read_size=(ssd->ave_read_size*ssd->read_request_count+request1->size)/(ssd->read_request_count+1);
      } 
   else
      {
         ssd->ave_write_size=(ssd->ave_write_size*ssd->write_request_count+request1->size)/(ssd->write_request_count+1);
      }

   filepoint = ftell(ssd->tracefile);  
   fgets(buffer, 200, ssd->tracefile);    //寻找下一条请求的到达时间
   sscanf(buffer,"%" PRId64 " %d %d %d %d",&time_t,&device,&lsn,&size,&ope);
   ssd->next_request_time=time_t;
   fseek(ssd->tracefile,filepoint,0);

   return 1;
}


/**********************************************************************************************************************************************
 *  首先buffer是个写buffer，就是为写请求服务的，因为读flash的时间tR为20us，写flash的时间tprog为200us，所以为写服务更能节省时间
 *  读操作：如果命中了buffer，从buffer读，不占用channel的I/O总线，没有命中buffer，从flash读，占用channel的I/O总线，但是不进buffer了
 *  写操作：首先request分成sub_request子请求，如果是动态分配，sub_request挂到ssd->sub_request上，因为不知道要先挂到哪个channel的sub_request上
 *          如果是静态分配则sub_request挂到channel的sub_request链上,同时不管动态分配还是静态分配sub_request都要挂到request的sub_request链上
 *       因为每处理完一个request，都要在traceoutput文件中输出关于这个request的信息。处理完一个sub_request,就将其从channel的sub_request链
 *       或ssd的sub_request链上摘除，但是在traceoutput文件输出一条后再清空request的sub_request链。
 *       sub_request命中buffer则在buffer里面写就行了，并且将该sub_page提到buffer链头(LRU)，若没有命中且buffer满，则先将buffer链尾的sub_request
 *       写入flash(这会产生一个sub_request写请求，挂到这个请求request的sub_request链上，同时视动态分配还是静态分配挂到channel或ssd的
 *       sub_request链上),在将要写的sub_page写入buffer链头
 ***********************************************************************************************************************************************/
struct ssd_info *buffer_management(struct ssd_info *ssd)
{   
   unsigned int j,lsn,lpn,last_lpn,first_lpn,index,complete_flag=0, state,full_page;
   unsigned int flag=0,need_distb_flag,lsn_flag,flag1=1,active_region_flag=0;           
   struct request *new_request;
   struct buffer_group *buffer_node,key;
   unsigned int mask=0,offset1=0,offset2=0;

   //printf("enter buffer_management,  current time:%lld\n",ssd->current_time);
   ssd->dram->current_time=ssd->current_time;
   full_page=~(0xffffffff<<ssd->parameter->subpage_page);
   
   new_request=ssd->request_tail;
   lsn=new_request->lsn;
   lpn=new_request->lsn/ssd->parameter->subpage_page;
   last_lpn=(new_request->lsn+new_request->size-1)/ssd->parameter->subpage_page;
   first_lpn=new_request->lsn/ssd->parameter->subpage_page;

   //new_request->need_distr_flag=(unsigned int*)malloc(sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));
   //unsigned int has 32 bit. each bit is used to represent the state of need_distr of a page. 
   new_request->need_distr_flag=(unsigned int*)calloc((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1, sizeof(unsigned int));  
   alloc_assert(new_request->need_distr_flag,"new_request->need_distr_flag");
   //memset(new_request->need_distr_flag, 0, sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));
   
   if(new_request->operation==READ) 
      {     
         while(lpn<=last_lpn)          
            {
               /************************************************************************************************
                *need_distb_flag表示是否需要执行distribution函数，1表示需要执行，buffer中没有，0表示不需要执行
                *即1表示需要分发，0表示不需要分发，对应点初始全部赋为1
                *************************************************************************************************/
               need_distb_flag=full_page;   
               key.group=lpn;
               buffer_node=(struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);      // buffer node 

               //printf("lpn: %d, buffer_node: %d\n", lpn, buffer_node);
               while((buffer_node!=NULL)&&(lsn<(lpn+1)*ssd->parameter->subpage_page)&&(lsn<=(new_request->lsn+new_request->size-1)))              
                  {
                     //printf("lsn: %d, lpn: %d, (lpn+1)*subpage_page: %d\n", lsn, lpn, (lpn+1)*ssd->parameter->subpage_page);
                     lsn_flag=full_page;
                     mask=1 << (lsn%ssd->parameter->subpage_page);
                     //printf("mask: %x, lsn: %d, shift: %d\n", mask, lsn, lsn%ssd->parameter->subpage_page);

                     //if(mask>31) //2KB page 
                     if (mask > 255) //4KB page  
                        {
                           printf("the subpage number is larger than 8!\n");
                           exit(1);          
                        }
                     else if((buffer_node->stored & mask)==mask)
                        {
                           flag=1;
                           //printf("lsn_flag: %x, mask: %x\n", lsn_flag, mask);
                           lsn_flag=lsn_flag&(~mask);
                        }

                     if(flag==1)          
                        {  //如果该buffer节点不在buffer的队首，需要将这个节点提到队首，实现了LRU算法，这个是一个双向队列。             
                           if(ssd->dram->buffer->buffer_head!=buffer_node)     
                              {     
                                 if(ssd->dram->buffer->buffer_tail==buffer_node)                      
                                    {        
                                       buffer_node->LRU_link_pre->LRU_link_next=NULL;              
                                       ssd->dram->buffer->buffer_tail=buffer_node->LRU_link_pre;                     
                                    }           
                                 else                       
                                    {           
                                       buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;          
                                       buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;                 
                                    }                       
                                 buffer_node->LRU_link_next=ssd->dram->buffer->buffer_head;
                                 ssd->dram->buffer->buffer_head->LRU_link_pre=buffer_node;
                                 buffer_node->LRU_link_pre=NULL;        
                                 ssd->dram->buffer->buffer_head=buffer_node;                                
                              }                 
                           ssd->dram->buffer->read_hit++;               
                           new_request->complete_lsn_count++;
                        }     
                     else if(flag==0)
                        {
                           ssd->dram->buffer->read_miss_hit++;
                        }

                     need_distb_flag=need_distb_flag&lsn_flag;
         
                     flag=0;
                     lsn++;                  
                  }

               index=(lpn-first_lpn)/(32/ssd->parameter->subpage_page);
               new_request->need_distr_flag[index]=new_request->need_distr_flag[index]|(need_distb_flag<<(((lpn-first_lpn)%(32/ssd->parameter->subpage_page))*ssd->parameter->subpage_page));         
               //printf("new_request->need_distr_flag[%d]: %x, need_distb_flag: %x\n", index, new_request->need_distr_flag[index], need_distb_flag);
               lpn++;

               // set lsn to lpn+1 if lpn with lsn does not hit in the buffer
               if (lsn != lpn*ssd->parameter->subpage_page)
                  lsn = lpn*ssd->parameter->subpage_page;//*/
            }
      }  
   else if(new_request->operation==WRITE)
      {
         while(lpn<=last_lpn)             
            {  
               //need_distb_flag=full_page;
               mask=~(0xffffffff<<(ssd->parameter->subpage_page));
               state=mask;

               if(lpn==first_lpn)
                  {
                     offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-new_request->lsn);
                     state=state&(0xffffffff<<offset1);
                  }
               if(lpn==last_lpn)
                  {
                     offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(new_request->lsn+new_request->size));
                     state=state&(~(0xffffffff<<offset2));
                  }

               ssd=insert2buffer(ssd, lpn, state,NULL,new_request);
               lpn++;
            }
      }
  
   complete_flag = 1;
   for(j=0;j<=(last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32;j++)
      {
         if(new_request->need_distr_flag[j] != 0)
            {
               complete_flag = 0;
            }
      }

   /*************************************************************
    *如果请求已经被全部由buffer服务，该请求可以被直接响应，输出结果
    *这里假设dram的服务时间为1000ns
    **************************************************************/
   //printf("new_request->subs: %d, lsn: %d, complete_flag: %d, operation: %d, size: %d, complete_lsn_count: %d\n", new_request->subs, new_request->lsn, complete_flag, new_request->operation, new_request->size, new_request->complete_lsn_count);
   if((complete_flag == 1)&&(new_request->subs==NULL))               
      {
         new_request->begin_time=ssd->current_time;
         new_request->response_time=ssd->current_time+1000;
      }

   return ssd;
}


/*****************************
 *lpn向ppn的转换
 ******************************/
unsigned int lpn2ppn(struct ssd_info *ssd,unsigned int lsn)
{
   int lpn, ppn;  
   struct entry *p_map = ssd->dram->map->map_entry;

   //printf("enter lpn2ppn,  current time:%lld\n",ssd->current_time);
  
   lpn = lsn/ssd->parameter->subpage_page;         //lpn
   ppn = (p_map[lpn]).pn;
   return ppn;
}


/**********************************************************************************
 *读请求分配子请求函数，这里只处理读请求，写请求已经在buffer_management()函数中处理了
 *根据请求队列和buffer命中的检查，将每个请求分解成子请求，将子请求队列挂在channel上，
 *不同的channel有自己的子请求队列
 **********************************************************************************/
struct ssd_info *distribute(struct ssd_info *ssd) 
{
   unsigned int start, end, first_lsn,last_lsn,lpn,flag=0,flag_attached=0,full_page;
   unsigned int j, k, sub_size;
   int i=0;
   struct request *req;
   struct sub_request *sub;
   int* complt;

   //printf("enter distribute,  current time:%lld\n",ssd->current_time);

   full_page=~(0xffffffff<<ssd->parameter->subpage_page);

   req = ssd->request_tail;
   if(req->response_time != 0){
      return ssd;
   }
   if (req->operation==WRITE)
      {
         return ssd;
      }

   if(req != NULL)
      {
         if(req->distri_flag == 0)
            {
               //如果还有一些读请求需要处理
               if(req->complete_lsn_count != ssd->request_tail->size)
                  {     
                     first_lsn = req->lsn;            
                     last_lsn = first_lsn + req->size;
                     complt = (int *)req->need_distr_flag;
                     start = first_lsn - first_lsn % ssd->parameter->subpage_page;
                     //end = (last_lsn/ssd->parameter->subpage_page + 1) * ssd->parameter->subpage_page;
                     end = last_lsn%ssd->parameter->subpage_page != 0 ? (last_lsn/ssd->parameter->subpage_page + 1) * ssd->parameter->subpage_page : last_lsn;
                     i = (end - start)/32;   

                     while(i >= 0)
                        {  
                           /*************************************************************************************
                            *一个32位的整型数据的每一位代表一个子页，32/ssd->parameter->subpage_page就表示有多少页，
                            *这里的每一页的状态都存放在了 req->need_distr_flag中，也就是complt中，通过比较complt的
                            *每一项与full_page，就可以知道，这一页是否处理完成。如果没处理完成则通过creat_sub_request
               函数创建子请求。
                           *************************************************************************************/
                           for(j=0; j<32/ssd->parameter->subpage_page; j++)
                              {  
                                 k = (complt[((end-start)/32-i)] >>(ssd->parameter->subpage_page*j)) & full_page; 
                                 if (k !=0)
                                    {
                                       lpn = start/ssd->parameter->subpage_page + ((end-start)/32-i)*32/ssd->parameter->subpage_page + j;
                                       sub_size=transfer_size(ssd,k,lpn,req);    
                                       if (sub_size==0) 
                                          {
                                             continue;
                                          }
                                       else
                                          {
                                             //sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation);
                                             sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation,0,0);
                                          }  
                                    }
                              }
                           i = i-1;
                        }

                  }
               else
                  {        
                     req->begin_time=ssd->current_time;
                     req->response_time=ssd->current_time+1000;         
                  }

            }
      }

   return ssd;
}


/**********************************************************************
 *trace_output()函数是在每一条请求的所有子请求经过process()函数处理完后，
 *打印输出相关的运行结果到outputfile文件中，这里的结果主要是运行的时间
 **********************************************************************/
void trace_output(struct ssd_info* ssd){
   int flag = 1;  
   int64_t start_time, end_time;
   struct request *req, *pre_node;
   struct sub_request *sub, *tmp;

   //printf("enter trace_output,  current time:%lld\n",ssd->current_time);

   pre_node=NULL;
   req = ssd->request_queue;
   start_time = 0;
   end_time = 0;

   if(req == NULL)
      return;

   while(req != NULL)   
      {
         sub = req->subs;
         flag = 1;
         start_time = 0;
         end_time = 0; 

         int first_sn = req->lsn/(ssd->parameter->subpage_page*(ssd->parameter->channel_number-1));
         int last_sn = (req->lsn + req->size - 1)/(ssd->parameter->subpage_page*(ssd->parameter->channel_number-1));
         //int seqs = last_sn - first_sn + 1;
      
         if(req->response_time != 0)
            {    
               fprintf(ssd->outputfile,"%16" PRId64 " %10d %6d %2d %16" PRId64 " %16" PRId64 " %10" PRId64 "\n",req->time,req->lsn, req->size, req->operation, req->begin_time, req->response_time, req->response_time-req->time);
               fflush(ssd->outputfile);
               //printf("trace_output/%lld, %d, %d, %d, %lld, %lld, %lld\n",req->time,req->lsn, req->size, req->operation, req->begin_time, req->response_time, req->response_time-req->time);
          
               if (req->time > ssd->stats_time)  { //for double workload experiment
                  fprintf(ssd->process_time, "%" PRId64 "\n", req->response_time - req->time);
                  if (req->operation)
                     fprintf(ssd->read_time, "%" PRId64 "\n", req->response_time - req->time);
                  //fprintf(ssd->read_time, "%lld\t%lld\n", req->time, req->response_time - req->time);
                  else
                     fprintf(ssd->write_time, "%" PRId64 "\n", req->response_time - req->time);

                  if (req->operation)
                     ssd->reqs_without_gc++;
               }
                 
               if(req->response_time-req->begin_time==0)
                  {
                     printf("the response time is 0??\n");
                     exit(1);
                  }

               if (req->time > ssd->stats_time) {
                  if (req->operation==READ)
                     {
                     
                        ssd->read_request_count++;
                        ssd->read_avg=ssd->read_avg+(req->response_time-req->time);                   
                     }
                  else
                     {         
                        ssd->write_request_count++;
                        ssd->write_avg=ssd->write_avg+(req->response_time-req->time);          
                     }
               }  

               if(pre_node == NULL)
                  {
                     if(req->next_node == NULL)
                        {
                           free(req->need_distr_flag);
                           req->need_distr_flag=NULL;

                           free(req->gc_count);
                           free(req->sub_req_dropped);
                           req->gc_count = NULL;
                           req->sub_req_dropped = NULL;
        
                           free(req);       
                           req = NULL;
                           ssd->request_queue = NULL;
                           ssd->request_tail = NULL;
                           ssd->request_queue_length--;
                        }
                     else
                        {
                           ssd->request_queue = req->next_node;
                           pre_node = req;
                           req = req->next_node;
                           free(pre_node->need_distr_flag);
                           pre_node->need_distr_flag=NULL;

                           free(pre_node->gc_count);
                           free(pre_node->sub_req_dropped);
                           pre_node->gc_count = NULL;
                           pre_node->sub_req_dropped = NULL;
        
                           free((void *)pre_node);
                           pre_node = NULL;
                           ssd->request_queue_length--;
                        }
                  }
               else
                  {
                     if(req->next_node == NULL)
                        {
                           pre_node->next_node = NULL;
                           free(req->need_distr_flag);
                           req->need_distr_flag=NULL;

                           free(req->gc_count);
                           free(req->sub_req_dropped);
                           req->gc_count = NULL;
                           req->sub_req_dropped = NULL;
        
                           free(req);
                           req = NULL;
                           ssd->request_tail = pre_node;
                           ssd->request_queue_length--;
                        }
                     else
                        {
                           pre_node->next_node = req->next_node;
                           free(req->need_distr_flag);
                           req->need_distr_flag=NULL;

                           free(req->gc_count);
                           free(req->sub_req_dropped);
                           req->gc_count = NULL;
                           req->sub_req_dropped = NULL;
        
                           free((void *)req);
                           req = pre_node->next_node;
                           ssd->request_queue_length--;
                        }
                  }
            }
         else
            {
               flag=1;
               /*int count = 0;
                 int changed = -1;
                 int sec_last_finished_first = 0; //second last returned sub_request finishes first in the seq read
                 int64_t end_time_seq = 0;
                 int64_t end_time_seq_ = 0;
                 int64_t *end_times = malloc(seqs*sizeof(int64_t));*/

               while(sub != NULL)
                  {
                     if(start_time == 0)
                        start_time = sub->begin_time;
                     if(start_time > sub->begin_time)
                        start_time = sub->begin_time;
                     if(end_time < sub->complete_time) 
                        end_time = sub->complete_time;

                     /*if(end_time_seq < sub->complete_time) {
                        end_time_seq_ = end_time_seq;
                        end_time_seq = sub->complete_time;
                        changed++;
                     }

                     if (!end_time_seq_)
                        end_time_seq_ = sub->complete_time;
                     if ((sub->complete_time < end_time_seq && sub->complete_time > end_time_seq_) || end_time_seq_ == end_time_seq) 
                        end_time_seq_= sub->complete_time;
                     if (end_time_seq == sub->complete_time && !changed)
                        sec_last_finished_first++;
         
                     count++;
                     if (count%ssd->parameter->channel_number == 0) {
                        if (sec_last_finished_first == ssd->parameter->channel_number - 1) 
                           end_time_seq_ = end_time_seq;    
                        end_times[count/ssd->parameter->channel_number - 1] = end_time_seq_;
                        end_time_seq = 0;
                        sec_last_finished_first = 0;
                        changed = -1;
                     }*/
         
                     //printf("seq: %d, seq_: %d\n", end_time_seq, end_time_seq_);        
                     if((sub->current_state == SR_COMPLETE)||((sub->next_state==SR_COMPLETE)&&(sub->next_state_predict_time<=ssd->current_time)))  // if any sub-request is not completed, the request is not completed
                        {       
                           //printf("trace_output/r/w: %d, parity: %d, lpn: %d, start time: %lld, end time: %lld, begin time: %lld, complete time: %lld, process time: %lld\n", sub->operation, sub->parity, sub->lpn, start_time, end_time, sub->begin_time, sub->complete_time, sub->complete_time - sub->begin_time);        
                           //printf("req_time: %d, start_time: %d, end_time: %d\n", req->time, start_time, end_time);
                           sub = sub->next_subs;
                        }
                     else
                        {
                           //printf("trace_output/r/w: %d, sub->lpn: %d, parity: %d\n", sub->operation, sub->lpn, sub->parity);
                           sub = sub->next_subs;
                           flag=0;         
                           //break;
                        }                 
                  }

               if (flag == 1)
                  {
                     //N/(N+1) is applied when either it's a proactive_read or writing phase of adaptive_read
                     /*if (ssd->parameter->proactive_read || (ssd->parameter->adaptive_read && !req->operation)) {
                        int i;
                        int64_t max = 0;
                        for (i = 0; i < seqs; i++) {
                           //printf("seqs: %d, time: %d\n", i, end_times[i]);
                           if (end_times[i] > max)
                              max = end_times[i];
                        }
                        end_time = max;
                     }*/
                     //printf("end_time: %lld\n", end_time);
         
                     //statistcs for read request
                     if (req->operation && req->time > ssd->stats_time) {
                        int i, k;
                        double j = 0;

                        k = last_sn - first_sn + 1;
                        ssd->groups_with_gc += k;
                        for (i = 0; i < last_sn - first_sn + 1; i++) {
                           j += req->gc_count[i];
                           fprintf(ssd->gc_per_group,"%d\n", req->gc_count[i]);
                           if (!req->gc_count[i]) {
                              k--;
                              ssd->groups_with_gc--;
                              ssd->groups_without_gc++;
                           }
                        }

                        fprintf(ssd->gc_per_req,"%d\n", (int)j);
                        if (j > 0) {
                           ssd->avg_gc_count += j/k;
                           ssd->reqs_with_gc++;
                        } else {
                           ssd->reqs_without_gc++;          
                        }
                     }
      
                     //fprintf(ssd->outputfile,"%10I64u %10u %6u %2u %16I64u %16I64u %10I64u\n",req->time,req->lsn, req->size, req->operation, start_time, end_time, end_time-req->time);
                     fprintf(ssd->outputfile,"%16" PRId64 " %10d %6d %2d %16" PRId64 " %16" PRId64 " %10" PRId64 "\n",req->time,req->lsn, req->size, req->operation, start_time, end_time, end_time-req->time);
                     fflush(ssd->outputfile);
                     //printf("trace_output/%lld, %d, %d, %d, %lld, %lld, %lld\n",req->time,req->lsn, req->size, req->operation, start_time, end_time, end_time - req->time);         

                     if (req->time > ssd->stats_time) {
                        fprintf(ssd->process_time, "%" PRId64 "\n", end_time - req->time);
                        if (req->operation == 1)
                           fprintf(ssd->read_time, "%" PRId64 "\n", end_time - req->time);
                        //fprintf(ssd->read_time, "%lld\t%lld\n", req->time, end_time - req->time);
                        else
                           fprintf(ssd->write_time, "%" PRId64 "\n", end_time - req->time);
                     }

                     if(end_time-req->time > 0.5*1E6)
                        ;//exit(1);
         
                     if(end_time-start_time==0)
                        {
                           printf("the response time is 0??\n");
                           exit(1);
                        }

                     if (req->time > ssd->stats_time) {
                        if (req->operation==READ)
                           {
                              ssd->read_request_count++;
                              ssd->read_avg=ssd->read_avg+(end_time-req->time);
                           } 
                        else
                           {
                              ssd->write_request_count++;
                              ssd->write_avg=ssd->write_avg+(end_time-req->time);
                           }
                     }

                     while(req->subs!=NULL)
                        {
                           tmp = req->subs;
                           //printf("lpn: %d, parity_read: %x, update: %x, parity: %d\n", tmp->lpn, tmp->parity_read, tmp->update, tmp->parity);
                           req->subs = tmp->next_subs;
                           if (tmp->update!=NULL)
                              //if (tmp->update!=NULL && tmp->current_state == SR_COMPLETE)
                              //if (tmp->update!=NULL && (ssd->parameter->advanced_commands&AD_COPYBACK)!=AD_COPYBACK) //since copyback is used, update is for read sub_request produced by parity update, which would later be freed by freeing parity_read.       
                              {            
                                 free(tmp->update->location);
                                 tmp->update->location=NULL;
                                 free(tmp->update);
                                 tmp->update=NULL;
                              }

                           //free space allocated to parity read
                           /*struct sub_request *curr;
                           while ((curr = tmp->parity_read) != NULL) {
                              tmp->parity_read = tmp->parity_read->next_subs;
                              free(curr);
                           }*/
        
                           //if (tmp->current_state == SR_COMPLETE) {
                           free(tmp->location);
                           tmp->location=NULL;
                           free(tmp);
                           tmp=NULL;
                           //}
                        }
            
                     if(pre_node == NULL)
                        {
                           if(req->next_node == NULL)
                              {           
                                 free(req->need_distr_flag);
                                 req->need_distr_flag=NULL;

                                 free(req->gc_count);
                                 free(req->sub_req_dropped);
                                 req->gc_count = NULL;
                                 req->sub_req_dropped = NULL;

                                 free(req);
                                 req = NULL;
                                 ssd->request_queue = NULL;
                                 ssd->request_tail = NULL;
                                 ssd->request_queue_length--;
                              }
                           else
                              {
                                 ssd->request_queue = req->next_node;
                                 pre_node = req;
                                 req = req->next_node;

                                 free(pre_node->gc_count);
                                 free(pre_node->sub_req_dropped);
                                 pre_node->gc_count = NULL;
                                 pre_node->sub_req_dropped = NULL;
            
                                 free(pre_node->need_distr_flag);
                                 pre_node->need_distr_flag = NULL;                        
                                 free(pre_node);
                                 pre_node = NULL;
                                 ssd->request_queue_length--;
                              }
                        }
                     else
                        {
                           if(req->next_node == NULL)
                              {
                                 pre_node->next_node = NULL;
                                 free(req->need_distr_flag);
                                 req->need_distr_flag=NULL;

                                 free(req->gc_count);
                                 free(req->sub_req_dropped);
                                 req->gc_count = NULL;
                                 req->sub_req_dropped = NULL;
            
                                 free(req);
                                 req = NULL;
                                 ssd->request_tail = pre_node; 
                                 ssd->request_queue_length--;
                              }
                           else
                              {
                                 pre_node->next_node = req->next_node;
                                 free(req->need_distr_flag);
                                 req->need_distr_flag=NULL;

                                 free(req->gc_count);
                                 free(req->sub_req_dropped);
                                 req->gc_count = NULL;
                                 req->sub_req_dropped = NULL;
            
                                 free(req);
                                 req = pre_node->next_node;
                                 ssd->request_queue_length--;
                              }

                        }
                  }
               else
                  {  
                     pre_node = req;
                     req = req->next_node;
                  }
            }
      }
}


/*******************************************************************************
 *statistic_output()函数主要是输出处理完一条请求后的相关处理信息。
 *1，计算出每个plane的擦除次数即plane_erase和总的擦除次数即erase
 *2，打印min_lsn，max_lsn，read_count，program_count等统计信息到文件outputfile中。
 *3，打印相同的信息到文件statisticfile中
 *******************************************************************************/
void statistic_output(struct ssd_info *ssd)
{
   //unsigned int lpn_count=0,i,j,k,m,erase=0,plane_erase=0;
   unsigned int lpn_count=0,i,j,k,m,n,erase=0,plane_erase=0;
   double gc_energy=0.0;
   //printf("enter statistic_output,  current time:%lld\n",ssd->current_time);

   /*for(i=0;i<ssd->parameter->channel_number;i++)
      {      
         for(j=0;j<ssd->parameter->die_chip;j++)
            {
               for(k=0;k<ssd->parameter->plane_die;k++)
                  {
                     plane_erase=0;
                     for(m=0;m<ssd->parameter->block_plane;m++)
                        {
                           if(ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count>0)
                              {
                                 erase=erase+ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count;
                                 plane_erase+=ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count;
                              }
                        }
                     fprintf(ssd->outputfile,"the %d channel, %d chip, %d die, %d plane has : %13d erase operations\n",i,j,k,m,plane_erase);
                     fprintf(ssd->statisticfile,"the %d channel, %d chip, %d die, %d plane has : %13d erase operations\n",i,j,k,m,plane_erase);
                  }
            }
      }//*/

   for(i = 0; i < ssd->parameter->channel_number; i++)
      {
         for (j = 0; j < ssd->parameter->chip_channel[i]; j++) {
            for(k = 0; k < ssd->parameter->die_chip;k++)
               {
                  for(m = 0; m < ssd->parameter->plane_die; m++)
                     {
                        plane_erase=0;
                        for(n = 0; n < ssd->parameter->block_plane; n++)
                           {
                              if(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[n].erase_count>0)
                                 {        
                                    erase+=ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[n].erase_count;
                                    plane_erase+=ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[n].erase_count;
                                 }
                           }
                        fprintf(ssd->outputfile,"channel: %d, chip: %d, die: %d, plane: %d, erase operations: %13d\n", i, j, k, m, plane_erase);
                        fprintf(ssd->statisticfile,"channel: %d, chip: %d, die: %d, plane: %d, erase operations: %13d\n", i, j, k, m, plane_erase);
                     }
               }
         }
      }
  
   fprintf(ssd->outputfile,"\n");
   fprintf(ssd->outputfile,"\n");
   fprintf(ssd->outputfile,"---------------------------statistic data---------------------------\n");  
   fprintf(ssd->outputfile,"min lsn: %13u\n",ssd->min_lsn); 
   fprintf(ssd->outputfile,"max lsn: %13u\n",ssd->max_lsn);
   fprintf(ssd->outputfile,"read count: %13lu\n",ssd->read_count);     
   fprintf(ssd->outputfile,"program count: %13lu",ssd->program_count);  
   fprintf(ssd->outputfile,"                        include the flash write count leaded by read requests\n");
   fprintf(ssd->outputfile,"the read operation leaded by un-covered update count: %13u\n",ssd->update_read_count);
   fprintf(ssd->outputfile,"erase count: %13lu\n",ssd->erase_count);
   fprintf(ssd->outputfile,"direct erase count: %13lu\n",ssd->direct_erase_count);
   fprintf(ssd->outputfile,"copy back count: %13lu\n",ssd->copy_back_count);
   fprintf(ssd->outputfile,"multi-plane program count: %13lu\n",ssd->m_plane_prog_count);
   fprintf(ssd->outputfile,"multi-plane read count: %13lu\n",ssd->m_plane_read_count);
   fprintf(ssd->outputfile,"interleave write count: %13lu\n",ssd->interleave_count);
   fprintf(ssd->outputfile,"interleave read count: %13lu\n",ssd->interleave_read_count);
   fprintf(ssd->outputfile,"interleave two plane and one program count: %13lu\n",ssd->inter_mplane_prog_count);
   fprintf(ssd->outputfile,"interleave two plane count: %13lu\n",ssd->inter_mplane_count);
   fprintf(ssd->outputfile,"gc copy back count: %13lu\n",ssd->gc_copy_back);
   fprintf(ssd->outputfile,"write flash count: %13lu\n",ssd->write_flash_count);
   fprintf(ssd->outputfile,"interleave erase count: %13lu\n",ssd->interleave_erase_count);
   fprintf(ssd->outputfile,"multiple plane erase count: %13lu\n",ssd->mplane_erase_conut);
   fprintf(ssd->outputfile,"interleave multiple plane erase count: %13lu\n",ssd->interleave_mplane_erase_count);
   fprintf(ssd->outputfile,"read request count: %13u\n",ssd->read_request_count);
   fprintf(ssd->outputfile,"write request count: %13u\n",ssd->write_request_count);
   fprintf(ssd->outputfile,"read request average size: %13f\n",ssd->ave_read_size);
   fprintf(ssd->outputfile,"write request average size: %13f\n",ssd->ave_write_size);
   if (ssd->read_request_count > 0)
      fprintf(ssd->outputfile,"read request average response time: %" PRId64 "\n",ssd->read_avg/ssd->read_request_count);
   if (ssd->write_request_count > 0)
      fprintf(ssd->outputfile,"write request average response time: %" PRId64 "\n",ssd->write_avg/ssd->write_request_count);
   fprintf(ssd->outputfile,"buffer read hits: %13lu\n",ssd->dram->buffer->read_hit);
   fprintf(ssd->outputfile,"buffer read miss: %13lu\n",ssd->dram->buffer->read_miss_hit);
   fprintf(ssd->outputfile,"buffer write hits: %13lu\n",ssd->dram->buffer->write_hit);
   fprintf(ssd->outputfile,"buffer write miss: %13lu\n",ssd->dram->buffer->write_miss_hit);
   //fprintf(ssd->outputfile,"erase: %13u\n",erase);
   fflush(ssd->outputfile);
   fclose(ssd->outputfile);


   fprintf(ssd->statisticfile,"\n");
   fprintf(ssd->statisticfile,"\n");
   fprintf(ssd->statisticfile,"---------------------------statistic data---------------------------\n"); 
   fprintf(ssd->statisticfile,"min lsn: %13u\n",ssd->min_lsn); 
   fprintf(ssd->statisticfile,"max lsn: %13u\n",ssd->max_lsn);
   fprintf(ssd->statisticfile,"read count: %13lu\n",ssd->read_count);     
   fprintf(ssd->statisticfile,"program count: %13lu",ssd->program_count);    
   fprintf(ssd->statisticfile,"                        include the flash write count leaded by read requests\n");
   fprintf(ssd->statisticfile,"the read operation leaded by un-covered update count: %13u\n",ssd->update_read_count);
   fprintf(ssd->statisticfile,"erase count: %13lu\n",ssd->erase_count);   
   fprintf(ssd->statisticfile,"direct erase count: %13lu\n",ssd->direct_erase_count);
   fprintf(ssd->statisticfile,"copy back count: %13lu\n",ssd->copy_back_count);
   fprintf(ssd->statisticfile,"multi-plane program count: %13lu\n",ssd->m_plane_prog_count);
   fprintf(ssd->statisticfile,"multi-plane read count: %13lu\n",ssd->m_plane_read_count);
   fprintf(ssd->statisticfile,"interleave count: %13lu\n",ssd->interleave_count);
   fprintf(ssd->statisticfile,"interleave read count: %13lu\n",ssd->interleave_read_count);
   fprintf(ssd->statisticfile,"interleave two plane and one program count: %13lu\n",ssd->inter_mplane_prog_count);
   fprintf(ssd->statisticfile,"interleave two plane count: %13lu\n",ssd->inter_mplane_count);
   fprintf(ssd->statisticfile,"gc copy back count: %13lu\n",ssd->gc_copy_back);
   fprintf(ssd->statisticfile,"write flash count: %13lu\n",ssd->write_flash_count);
   fprintf(ssd->statisticfile,"waste page count: %13lu\n",ssd->waste_page_count);
   fprintf(ssd->statisticfile,"interleave erase count: %13lu\n",ssd->interleave_erase_count);
   fprintf(ssd->statisticfile,"multiple plane erase count: %13lu\n",ssd->mplane_erase_conut);
   fprintf(ssd->statisticfile,"interleave multiple plane erase count: %13lu\n",ssd->interleave_mplane_erase_count);
   fprintf(ssd->statisticfile,"read request count: %13u\n",ssd->read_request_count);
   fprintf(ssd->statisticfile,"write request count: %13u\n",ssd->write_request_count);
   fprintf(ssd->statisticfile,"read request average size: %13f\n",ssd->ave_read_size);
   fprintf(ssd->statisticfile,"write request average size: %13f\n",ssd->ave_write_size);
   if (ssd->read_request_count > 0)
      fprintf(ssd->statisticfile,"read request average response time: %" PRId64 "\n",ssd->read_avg/ssd->read_request_count);
   if (ssd->write_request_count > 0)
      fprintf(ssd->statisticfile,"write request average response time: %" PRId64 "\n",ssd->write_avg/ssd->write_request_count);
   fprintf(ssd->statisticfile,"buffer read hits: %13lu\n",ssd->dram->buffer->read_hit);
   fprintf(ssd->statisticfile,"buffer read miss: %13lu\n",ssd->dram->buffer->read_miss_hit);
   fprintf(ssd->statisticfile,"buffer write hits: %13lu\n",ssd->dram->buffer->write_hit);
   fprintf(ssd->statisticfile,"buffer write miss: %13lu\n",ssd->dram->buffer->write_miss_hit);
   //fprintf(ssd->statisticfile,"erase: %13d\n",erase);
   fflush(ssd->statisticfile); 
   fclose(ssd->statisticfile);

   int free_pages = 0;
   for (i=0;i<ssd->parameter->channel_number;i++)
      for (j=0;j<ssd->parameter->chip_channel[i];j++)
         for (k=0;k<ssd->parameter->die_chip;k++)
            for (m=0;m<ssd->parameter->plane_die;m++) {         
               fprintf(ssd->gc_ratio,"%.6f\n", (double)ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].free_page/(ssd->parameter->page_block*ssd->parameter->block_plane));
               free_pages += ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].free_page;
            }
   //free_pages -= (int)(64*ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold);

   /*for (i=0;i<ssd->parameter->channel_number;i++)
      for (j=0;j<ssd->parameter->chip_channel[i];j++)
         printf("%.6f\n", (double)ssd->channel_head[i].chip_head[j].program_count);//*/
          
   printf("\nwrite request average response time: %" PRId64 " us\nread request average response time: %" PRId64 " us\n", ssd->write_request_count > 0 ? ssd->write_avg/ssd->write_request_count/1000 : 0, ssd->read_request_count > 0 ? ssd->read_avg/ssd->read_request_count/1000 : 0);
   printf("\nread reqs_without_gc: %d, read reqs_with_gc: %d, groups_without_gc: %d, groups_with_gc: %d, avg_gc_count/group: %.3f\n", ssd->reqs_without_gc, ssd->reqs_with_gc, ssd->groups_without_gc, ssd->groups_with_gc, ssd->reqs_with_gc ? ssd->avg_gc_count/ssd->reqs_with_gc : 0);
   printf("\ngc_added: %d, gc_completed: %d, gc_in_progress: %d, uninterrupt_gc_deleted: %d, interrupt_gc_deleted: %d, gc_foreground: %d, gc_background: %d, copy_back_count: %lu, erase_count: %lu, direct_erase_count: %lu\n", ssd->gc_added, ssd->gc_completed, ssd->uninterrupt_gc_deleted + ssd->interrupt_gc_deleted - ssd->gc_completed, ssd->uninterrupt_gc_deleted, ssd->interrupt_gc_deleted, ssd->gc_foreground, ssd->gc_background, ssd->copy_back_count, ssd->erase_count, ssd->direct_erase_count);  
   printf("\nread requests with GC: %.3f %%, read requests with queuing delay: %.3f %%, total: %.3f %%, merge_read: %d\n", ssd->read_request_count > 0 ? (double)ssd->reqs_with_gc/(ssd->reqs_without_gc + ssd->reqs_with_gc)*100 : 0, ssd->read_request_count > 0 ? (double)ssd->delayed_requests/(ssd->reqs_without_gc + ssd->reqs_with_gc)*100 : 0, ssd->read_request_count > 0 ? (double)(ssd->delayed_requests + ssd->reqs_with_gc)/(ssd->reqs_without_gc + ssd->reqs_with_gc)*100 : 0, ssd->merge_read);
   printf("\nssd->current_time: %" PRId64 " ns, ssd->start_time: %" PRId64 " ns, ssd->current_time - ssd->start_time: %" PRId64 " ns\n", ssd->current_time, ssd->start_time, ssd->current_time - ssd->start_time);  
   //printf("\nfree_pages - threshold: %d, free_blocks - threshold: %d\n", free_pages, free_pages/(int)(ssd->parameter->page_block));
   
   fclose(ssd->gc_per_group);
   fclose(ssd->gc_per_req);
   fclose(ssd->process_time);
   fclose(ssd->gc_ratio);
}


/***********************************************************************************
 *根据每一页的状态计算出每一需要处理的子页的数目，也就是一个子请求需要处理的子页的页数
 ************************************************************************************/
unsigned int size(unsigned int stored)
{
   unsigned int i,total=0,mask=0x80000000;
   //printf("enter size\n");

   for(i=1;i<=32;i++)
      {
         if(stored & mask) total++;
         stored<<=1;
      }

   //printf("leave size\n");  
   return total;
}


/*********************************************************
 *transfer_size()函数的作用就是计算出子请求的需要处理的size
 *函数中单独处理了first_lpn，last_lpn这两个特别情况，因为这
 *两种情况下很有可能不是处理一整页而是处理一页的一部分，因
 *为lsn有可能不是一页的第一个子页。
 *********************************************************/
unsigned int transfer_size(struct ssd_info *ssd,int need_distribute,unsigned int lpn,struct request *req)
{
   unsigned int first_lpn,last_lpn,state,trans_size;
   unsigned int mask=0,offset1=0,offset2=0;

   first_lpn=req->lsn/ssd->parameter->subpage_page;
   last_lpn=(req->lsn+req->size-1)/ssd->parameter->subpage_page;

   mask=~(0xffffffff<<(ssd->parameter->subpage_page));
   state=mask;
   if(lpn==first_lpn)
      {
         offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-req->lsn);
         state=state&(0xffffffff<<offset1);
      }
   if(lpn==last_lpn)
      {
         offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(req->lsn+req->size));
         state=state&(~(0xffffffff<<offset2));
      }

   trans_size=size(state&need_distribute);

   return trans_size;
}


/**********************************************************************************************************  
 *int64_t find_nearest_event(struct ssd_info *ssd)       
 *寻找所有子请求的最早到达的下个状态时间,首先看请求的下一个状态时间，如果请求的下个状态时间小于等于当前时间，
 *说明请求被阻塞，需要查看channel或者对应die的下一状态时间。Int64是有符号 64 位整数数据类型，值类型表示值介于
 *-2^63 ( -9,223,372,036,854,775,808)到2^63-1(+9,223,372,036,854,775,807 )之间的整数。存储空间占 8 字节。
 *channel,die是事件向前推进的关键因素，三种情况可以使事件继续向前推进，channel，die分别回到idle状态，die中的
 *读数据准备好了
 ***********************************************************************************************************/
int64_t find_nearest_event(struct ssd_info *ssd) 
{
   unsigned int i,j;
   int64_t time=MAX_INT64;
   int64_t time1=MAX_INT64;
   int64_t time2=MAX_INT64;

   for (i=0;i<ssd->parameter->channel_number;i++)
      {
         if (ssd->channel_head[i].next_state==CHANNEL_IDLE)
            if(time1>ssd->channel_head[i].next_state_predict_time)
               if (ssd->channel_head[i].next_state_predict_time>ssd->current_time)    
                  time1=ssd->channel_head[i].next_state_predict_time;
         for (j=0;j<ssd->parameter->chip_channel[i];j++)
            {
               if ((ssd->channel_head[i].chip_head[j].next_state==CHIP_IDLE)||(ssd->channel_head[i].chip_head[j].next_state==CHIP_DATA_TRANSFER))
                  if(time2>ssd->channel_head[i].chip_head[j].next_state_predict_time)           
                     if (ssd->channel_head[i].chip_head[j].next_state_predict_time>ssd->current_time)        
                        time2=ssd->channel_head[i].chip_head[j].next_state_predict_time;       
            }  
      } 

   /*****************************************************************************************************
    *time为所有 A.下一状态为CHANNEL_IDLE且下一状态预计时间大于ssd当前时间的CHANNEL的下一状态预计时间
    *           B.下一状态为CHIP_IDLE且下一状态预计时间大于ssd当前时间的DIE的下一状态预计时间
    *        C.下一状态为CHIP_DATA_TRANSFER且下一状态预计时间大于ssd当前时间的DIE的下一状态预计时间
    *CHIP_DATA_TRANSFER读准备好状态，数据已从介质传到了register，下一状态是从register传往buffer中的最小值 
    *注意可能都没有满足要求的time，这时time返回0x7fffffffffffffff 。
    *****************************************************************************************************/
   time=(time1>time2)?time2:time1;
   return time;
}


/***********************************************
 *free_all_node()函数的作用就是释放所有申请的节点
 ************************************************/
void free_all_node(struct ssd_info *ssd)
{
   unsigned int i,j,k,l,n;
   struct buffer_group *pt=NULL;
   struct direct_erase * erase_node=NULL;
   for (i=0;i<ssd->parameter->channel_number;i++)
      {
         for (j=0;j<ssd->parameter->chip_channel[0];j++)
            {
               for (k=0;k<ssd->parameter->die_chip;k++)
                  {
                     for (l=0;l<ssd->parameter->plane_die;l++)
                        {
                           for (n=0;n<ssd->parameter->block_plane;n++)
                              {
                                 free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head);
                                 ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head=NULL;
                              }
                           free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head);
                           ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head=NULL;
                           while(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node!=NULL)
                              {
                                 erase_node=ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node;
                                 ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node=erase_node->next_node;
                                 free(erase_node);
                                 erase_node=NULL;
                              }
                        }
            
                     free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head);
                     ssd->channel_head[i].chip_head[j].die_head[k].plane_head=NULL;
                  }
               free(ssd->channel_head[i].chip_head[j].die_head);
               ssd->channel_head[i].chip_head[j].die_head=NULL;
            }
         free(ssd->channel_head[i].chip_head);
         ssd->channel_head[i].chip_head=NULL;
      }
   free(ssd->channel_head);
   ssd->channel_head=NULL;

   avlTreeDestroy( ssd->dram->buffer);
   ssd->dram->buffer=NULL;
   
   free(ssd->dram->map->map_entry);
   free(ssd->dram->map->parity_entry);
   ssd->dram->map->map_entry=NULL;
   ssd->dram->map->parity_entry=NULL;
   free(ssd->dram->map);
   ssd->dram->map=NULL;
   free(ssd->dram);
   ssd->dram=NULL;
   free(ssd->parameter);
   ssd->parameter=NULL;

   free(ssd);
   ssd=NULL;
}


/*****************************************************************************
 *make_aged()函数的作用就死模拟真实的用过一段时间的ssd，
 *那么这个ssd的相应的参数就要改变，所以这个函数实质上就是对ssd中各个参数的赋值。
 ******************************************************************************/
struct ssd_info *make_aged(struct ssd_info *ssd)
{
   unsigned int i,j,k,l,m,n,ppn;
   int threshold,flag=0;
  
   if (ssd->parameter->aged==1)
      {
         //threshold表示一个plane中有多少页需要提前置为失效
         //threshold=(int)(ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->aged_ratio); //def
         threshold=(int)(ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->aged_ratio/2);
         //threshold=(int)(ssd->parameter->block_plane*ssd->parameter->page_block*0.1745); //LMBE
         for (i=0;i<ssd->parameter->channel_number;i++)
            for (j=0;j<ssd->parameter->chip_channel[i];j++)
               for (k=0;k<ssd->parameter->die_chip;k++)
                  for (l=0;l<ssd->parameter->plane_die;l++)
                     {  
                        flag=0;
                        //double aged_ratio = (65 + rand()%11)/100.0;
                        //threshold = (int)(ssd->parameter->block_plane*ssd->parameter->page_block*aged_ratio);
                        //printf("threshold: %f\n",(double)threshold/(ssd->parameter->block_plane*ssd->parameter->page_block));
                        for (m=0;m<ssd->parameter->block_plane;m++)
                           {
                              if (flag>=threshold)
                                 {
                                    break;
                                 }

                              //printf("treshold: %f\n", (ssd->parameter->page_block*ssd->parameter->aged_ratio+1));
                              for (n=0;n<(ssd->parameter->page_block*ssd->parameter->aged_ratio+1);n++)
                                 //for (n=0;n<(ssd->parameter->page_block*aged_ratio+1);n++)          
                                 {  
                                    ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].valid_state=0;        //表示某一页失效，同时标记valid和free状态都为0
                                    ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].free_state=0;         //表示某一页失效，同时标记valid和free状态都为0
                                    ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].lpn=0;  //把valid_state free_state lpn都置为0表示页失效，检测的时候三项都检测，单独lpn=0可以是有效页
                                    ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_page_num--;
                                    ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].invalid_page_num++;
                                    ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].last_write_page++;
                                    ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].free_page--;
                                    flag++;

                                    ppn=find_ppn(ssd,i,j,k,l,m,n);                     
                                 }
                              //printf("channel: %d, chip: %d, invalid_page: %d, free_page: %d\n", i, j, ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].invalid_page_num, ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_page_num);
                           }

                        //printf("# of free pages/chip: %d, # of pages/chip: %d, threshold: %d, free page percent: %f, # of modified blocks: %d\n",ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].free_page, ssd->parameter->block_plane*ssd->parameter->page_block, threshold, (double)ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].free_page/(ssd->parameter->block_plane*ssd->parameter->page_block), m);    
                     }
      }  
   else
      {
         return ssd;
      }

   return ssd;
}


/*********************************************************************************************
 *no_buffer_distribute()函数是处理当ssd没有dram的时候，
 *这是读写请求就不必再需要在buffer里面寻找，直接利用creat_sub_request()函数创建子请求，再处理。
 *********************************************************************************************/
struct ssd_info *no_buffer_distribute(struct ssd_info *ssd)
{
  
   unsigned int lsn,lpn,last_lpn,first_lpn,complete_flag=0, state;
   unsigned int flag=0,flag1=1,active_region_flag=0;           //to indicate the lsn is hitted or not
   struct request *req=NULL;
   struct sub_request *sub=NULL,*sub_r=NULL,*update=NULL;
   //struct local *loc=NULL;
   struct channel_info *p_ch=NULL;
   
   unsigned int mask=0; 
   unsigned int offset1=0, offset2=0;
   unsigned int sub_size=0;
   unsigned int sub_state=0;

   ssd->dram->current_time=ssd->current_time;
   req=ssd->request_tail;       
   lsn=req->lsn;
   lpn=req->lsn/ssd->parameter->subpage_page;
   last_lpn=(req->lsn+req->size-1)/ssd->parameter->subpage_page;
   first_lpn=req->lsn/ssd->parameter->subpage_page;
  
   if(req->operation==READ)        
      {
         while(lpn<=last_lpn)       
            {    
               sub_state=(ssd->dram->map->map_entry[lpn].state&0x7fffffff);
               sub_size=size(sub_state);
               //sub=creat_sub_request(ssd,lpn,sub_size,sub_state,req,req->operation);
               sub=creat_sub_request(ssd,lpn,sub_size,sub_state,req,req->operation,0,0);
               if (ssd->parameter->proactive_read || ssd->parameter->adaptive_read) {
                  if ((lpn + 1)%7 == 0) {
                     sub_state=(ssd->dram->map->parity_entry[(lpn-6)/7].state&0x7fffffff);
                     sub_size=size(sub_state);
                     creat_sub_request(ssd,(lpn-6)/7,sub_size,sub_state,req,req->operation,1,0);
                  }
               }
               lpn++;
            }
      }
   else if(req->operation==WRITE)
      {
         while(lpn<=last_lpn)       
            {
               mask=~(0xffffffff<<(ssd->parameter->subpage_page));
               state=mask;
               if(lpn==first_lpn)
                  {
                     offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-req->lsn);
                     state=state&(0xffffffff<<offset1);
                  }
               if(lpn==last_lpn)
                  {
                     offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(req->lsn+req->size));
                     state=state&(~(0xffffffff<<offset2));
                  }
               sub_size=size(state);

               //sub=creat_sub_request(ssd,lpn,sub_size,state,req,req->operation);
               creat_sub_request(ssd,lpn,sub_size,state,req,req->operation,0,0);   
               if (ssd->parameter->raid) {
                  if ((lpn + 1)%7 == 0) {
                     creat_sub_request(ssd,lpn,sub_size,state,req,req->operation,1,0);
                  }
               }
               lpn++;
            }
      }

   return ssd;
}


struct ssd_info *buffer_management_modified(struct ssd_info *ssd)
{   
   //unsigned int j,lsn,lpn,last_lpn,first_lpn,index,complete_flag=0,state,full_page;
   unsigned int i,j,lsn,lpn,last_lpn,first_lpn,index,complete_flag=0,state,full_page,sn,last_sn,first_sn; //sn: stripe number

   //unsigned int flag=0,need_distb_flag,lsn_flag,flag1=1,active_region_flag=0;
   unsigned int flag = 0, lsn_flag, flag1=1, active_region_flag = 0;
   unsigned int need_distb_flag[2] = {0};
  
   struct request *new_request;
   struct buffer_group *buffer_node,key;
   unsigned int mask=0,offset1=0,offset2=0;

   //printf("enter buffer_management,  current time:%lld\n",ssd->current_time);

   ssd->dram->current_time=ssd->current_time;
   full_page=~(0xffffffff<<ssd->parameter->subpage_page);
   
   new_request=ssd->request_tail;
   lsn=new_request->lsn;

   lpn=new_request->lsn/ssd->parameter->subpage_page;
   sn = new_request->lsn/(ssd->parameter->subpage_page*(ssd->parameter->channel_number-1));

   last_lpn=(new_request->lsn+new_request->size-1)/ssd->parameter->subpage_page;
   last_sn = (new_request->lsn+new_request->size-1)/(ssd->parameter->subpage_page*(ssd->parameter->channel_number-1));

   first_lpn=new_request->lsn/ssd->parameter->subpage_page;
   first_sn = new_request->lsn/(ssd->parameter->subpage_page*(ssd->parameter->channel_number-1));
   //printf("first_sn: %u, last_sn: %u, new_request->lsn: %u, new_request->size: %u\n", first_sn, last_sn, new_request->lsn, new_request->size);
   
   //new_request->need_distr_flag=(unsigned int*)malloc(sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));
   //unsigned int has 32 bit. each bit is used to represent the state of need_distr of a page. 
   //new_request->need_distr_flag=(unsigned int*)calloc((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1, sizeof(unsigned int));
   new_request->need_distr_flag = (unsigned int *)calloc((last_sn - first_sn + 1)*ssd->parameter->subpage_page*2, sizeof(unsigned int));//hardcoded, each stripe would require 2 int to store its states
   alloc_assert(new_request->need_distr_flag,"new_request->need_distr_flag");
   //memset(new_request->need_distr_flag, 0, sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));
   
   if(new_request->operation==READ) 
      {
         //while(lpn<=last_lpn)
         while(sn<=last_sn)            
            {
               //need_distb_flag=full_page; 
               need_distb_flag[0] = 0xffffffff; //represent states of 1st 4 pages
               need_distb_flag[1] = 0x00ffffff; //represent states of 2nd 3 pages     
     
               //key.group=lpn;
               key.group=sn;
               buffer_node=(struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);      //buffer node 

               //printf("first_sn: %d, sn: %d, last_sn: %d, buffer_node: %d\n", first_sn, sn, last_sn, buffer_node);
               //while((buffer_node!=NULL)&&(lsn<(lpn+1)*ssd->parameter->subpage_page)&&(lsn<=(new_request->lsn+new_request->size-1)))
               while((buffer_node!=NULL)&&(lsn<(sn+1)*ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1))&&(lsn<=(new_request->lsn+new_request->size-1)))            
                  {
                     //printf("lsn: %d, sn: %d, (sn+1)*subpage_page*(channel_num - 1): %d\n", lsn, sn, (sn+1)*ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1));

                     //lsn_flag=full_page;
                     if ((lsn - sn*56)/32 != 1) 
                        lsn_flag = 0xffffffff;
                     else
                        lsn_flag = 0x00ffffff;
         
                     //mask=1 << (lsn%ssd->parameter->subpage_page);
                     unsigned int shift = lsn%(ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1));
                     mask = 1 << (shift > 31 ? shift - 32 : shift); // if shift > 31, it means we should mask the states of 2nd 3 pages 
                     //printf("mask: %x, lsn: %d, shift: %d\n", mask, lsn, shift);

                     if ((buffer_node->stored_s[shift > 31 ? 1 : 0] & mask) == mask) {
                        flag=1;
                        lsn_flag=lsn_flag&(~mask);
                        //printf("lsn_flag: %x, mask: %x\n", lsn_flag, mask);
                     }

                     if(flag==1)
                        {  //如果该buffer节点不在buffer的队首，需要将这个节点提到队首，实现了LRU算法，这个是一个双向队列。          
                           if(ssd->dram->buffer->buffer_head!=buffer_node)     
                              {     
                                 if(ssd->dram->buffer->buffer_tail==buffer_node)                      
                                    {        
                                       buffer_node->LRU_link_pre->LRU_link_next=NULL;              
                                       ssd->dram->buffer->buffer_tail=buffer_node->LRU_link_pre;                     
                                    }           
                                 else                       
                                    {           
                                       buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;          
                                       buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;                       
                                    }                       
                                 buffer_node->LRU_link_next=ssd->dram->buffer->buffer_head;
                                 ssd->dram->buffer->buffer_head->LRU_link_pre=buffer_node;
                                 buffer_node->LRU_link_pre=NULL;        
                                 ssd->dram->buffer->buffer_head=buffer_node;                                      
                              }                 
                           ssd->dram->buffer->read_hit++;               
                           new_request->complete_lsn_count++;
                        }     
                     else if(flag==0)
                        {
                           ssd->dram->buffer->read_miss_hit++;
                        }

                     if ((lsn - sn*56)/32 != 1)
                        need_distb_flag[0]=need_distb_flag[0]&lsn_flag;
                     else
                        need_distb_flag[1]=need_distb_flag[1]&lsn_flag;
         
                     flag=0;
                     lsn++;                  
                  }      

               //index=(lpn-first_lpn)/(32/ssd->parameter->subpage_page);
               index = sn - first_sn; //for each sn, it takes 2 int to store the states of its pages.
     
               //new_request->need_distr_flag[index]=new_request->need_distr_flag[index]|(need_distb_flag<<(((lpn-first_lpn)%(32/ssd->parameter->subpage_page))*ssd->parameter->subpage_page));
               new_request->need_distr_flag[2*index] = need_distb_flag[0]; 
               new_request->need_distr_flag[2*index + 1] = need_distb_flag[1];

               //printf("new_request->need_distr_flag[%d]: %x, new_request->need_distb_flag[%d]: %x\n", 2*index, new_request->need_distr_flag[2*index], 2*index + 1, new_request->need_distr_flag[2*index + 1]);
               //lpn++;
               sn++;

               // set lsn to lpn+1 if lpn with lsn does not hit in the buffer
               //if (lsn != lpn*ssd->parameter->subpage_page)
               if (lsn != sn*(ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1)))
                  lsn = sn*(ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1));
               //lsn = lpn*ssd->parameter->subpage_page;

            }
      }
   else if(new_request->operation==WRITE)
      {
         while(lpn<=last_lpn)             
            {  
               //need_distb_flag=full_page;
               mask=~(0xffffffff<<(ssd->parameter->subpage_page));
               state=mask;

               if(lpn==first_lpn)
                  {
                     offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-new_request->lsn); 
                     state=state&(0xffffffff<<offset1); 
                  }
               if(lpn==last_lpn)   
                  {
                     offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(new_request->lsn+new_request->size));
                     state=state&(~(0xffffffff<<offset2));
                  }

               //printf("buffer_management_modified/lpn: %d, sn: %d, state: %x\n", lpn, sn, state);
               ssd=insert2buffer_modified(ssd,lpn,state,NULL,new_request); 
               lpn++;
            }
      }

   complete_flag = 1;
   //for(j=0;j<=(last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32;j++)
   for (i = 0; i < last_sn - first_sn + 1; i++)
      {
         /*if(new_request->need_distr_flag[j] != 0)      
           {
           complete_flag = 0;
           }*/
      
         //we should check lpn on sn instead of simply sn
         for (j = 0; j < ssd->parameter->channel_number - 1; j++) {
            lpn = (i + first_sn)*(ssd->parameter->channel_number - 1) + j;
            if (first_lpn <= lpn && lpn <= last_lpn) {
               unsigned int shift = j < 4 ? j : j - 4;
               //printf("buffer_management_modified/complete_check, lpn: %d, first_lpn: %d, last_lpn: %d, need_distr_flag(read): %x\n", lpn, first_lpn, last_lpn, (new_request->need_distr_flag[j < 4 ? 2*i : 2*i + 1] >> shift*ssd->parameter->subpage_page) & 0x000000ff);
               if (((new_request->need_distr_flag[j < 4 ? 2*i : 2*i + 1] >> shift*ssd->parameter->subpage_page) & 0x000000ff) != 0) {
                  complete_flag = 0;
                  //break;
               }
            }
         }  
      }

   //printf("new_request->subs: %d, lsn: %d, complete_flag: %d, operation: %d, size: %d, complete_lsn_count: %d\n", new_request->subs, new_request->lsn, complete_flag, new_request->operation, new_request->size, new_request->complete_lsn_count);
   if((complete_flag == 1)&&(new_request->subs==NULL))               
      {
         new_request->begin_time=ssd->current_time;
         new_request->response_time=ssd->current_time+1000;
      }

   return ssd;
}//*/


//calculate state by or with page states of first or last sn
unsigned int calc_state(struct ssd_info *ssd, char *state_0, char *state_1, unsigned int first_lpn, unsigned int last_lpn, unsigned int first_sn, unsigned int last_sn, int first) {
   unsigned int i, j;
   unsigned int state = 0;
   if (first) {
      i = first_lpn;
      while (i < (first_sn + 1)*(ssd->parameter->channel_number - 1)) {
         j = i - first_sn*(ssd->parameter->channel_number - 1);
         if (j < 4)
            state = state | state_0[j];
         else
            state = state | state_0[j - 4];
         i++;
      }
   } else {
      i = last_sn*(ssd->parameter->channel_number - 1);
      while (i <= last_lpn) {
         j = i - last_sn*(ssd->parameter->channel_number - 1);
         if (j < 4)
            state = state | state_0[j];
         else
            state = state | state_0[j - 4];
         i++;
      }
   }

   return state;
}


//decide whether to convert partial RAID group read to RAID group read.
int convert_to_seq(struct ssd_info *ssd, unsigned int first_lpn, unsigned int last_lpn, unsigned int sn, unsigned int data_transfer_size, int opt, struct request *req) {
   //return 1;//blindly convert_to_seq
   //disable TTPartialRead
   if (ssd->parameter->tt_pread == 0) 
      return 0;
      
   int flag1 = 0, flag2 = 0, extra_reads = 0, i, start, end;
   int64_t cost = 0, max_in = 0, max_out = 0; // max GC in/out the read region

   unsigned int parity_ch, first_ch, last_ch, max_ch;
   struct local *parity_loc = find_location(ssd, ssd->dram->map->parity_entry[sn].pn);
   struct local *first_loc = find_location(ssd, ssd->dram->map->map_entry[first_lpn].pn);
   struct local *last_loc = find_location(ssd, ssd->dram->map->map_entry[last_lpn].pn);
   parity_ch = parity_loc->channel;
   first_ch = first_loc->channel;
   last_ch = last_loc->channel;
   
   if (opt == 1) {
      start = first_ch;
      end = 7; //span of this small read
   } else if (opt == 2) {
      start = 0;
      end = last_ch;
   } else {
      start = first_ch;
      end = last_ch;
      //printf("start: %d, end: %d, parity_ch: %d, first_lpn: %d, last_lpn: %d, data_transfer_size: %d\n", start, end, parity_ch, first_lpn, last_lpn, data_transfer_size);
   }     
      
   for (i = 0; i < ssd->parameter->channel_number; i++) {
      if ((ssd->dram->gc_monitor[i][sn%ssd->parameter->chip_channel[0]].next_state_predict_time || ssd->dram->gc_monitor[i][sn%ssd->parameter->chip_channel[0]].valid_pages != 0)) {
         if (i >= start && i < end + 1) {
            if (!ssd->dram->rotating_gc) {//no rotating GC, use coarse grained gc_predict_complete_time to estimate the cost (since the cost could be > 0 in copy phase. with rotating GC, the cost is always < 0 if GC is in copy phase)
               flag2 = 1;
               //cost = -(ssd->dram->gc_monitor[i][sn%ssd->parameter->chip_channel[0]].next_state_predict_time - ssd->current_time); //average latency due to gc
               if (ssd->dram->gc_monitor[i][sn%ssd->parameter->chip_channel[0]].gc_predict_complete_time - ssd->current_time > max_in)
                  max_in = ssd->dram->gc_monitor[i][sn%ssd->parameter->chip_channel[0]].gc_predict_complete_time - ssd->current_time;
            } else if (i != parity_ch) { //with rotating GC, use fine grained next_state_predict_time to estimate the cost
               //if there is GC and this GC is erasing the block, flag is set to be 1. otherwise valid_pages != 0, which means there are still copyback operations. in this case, it's always better to convert partial RAID group read to RAID group read              
               flag1 = 1;
               if (ssd->dram->gc_monitor[i][sn%ssd->parameter->chip_channel[0]].valid_pages == 0) {
                  flag2 = 1;
                  cost = -(ssd->dram->gc_monitor[i][sn%ssd->parameter->chip_channel[0]].next_state_predict_time - ssd->current_time); //average latency due to GC
                  break;
               }
            }
         } else {
            if (ssd->dram->gc_monitor[i][sn%ssd->parameter->chip_channel[0]].gc_predict_complete_time - ssd->current_time > max_out)
               max_out = ssd->dram->gc_monitor[i][sn%ssd->parameter->chip_channel[0]].gc_predict_complete_time - ssd->current_time;
         }
      }
   }

   if (ssd->parameter->dynamic_rotating_gc ? !ssd->dram->rotating_gc : !ssd->parameter->rotating_gc)
      cost = max_out - max_in;
   //printf("cost: %" PRId64 ", max_out: %" PRId64 ", max_in: %" PRId64 "\n", cost, max_out, max_in);
   
   if (flag2) {
      //if this is first_sn and first_sn != last_sn
      if (opt == 1) { 
         extra_reads = first_lpn - sn*(ssd->parameter->channel_number - 1) + 1; //+1 because of parity read  
      } else if (opt == 2){ //else if this is last_sn and first_sn != last_sn
         extra_reads = (sn + 1)*(ssd->parameter->channel_number - 1) - last_lpn;  
      } else { //first_sn == last_sn
         extra_reads = (ssd->parameter->channel_number - 1) - (last_lpn - first_lpn);
         if (last_lpn - first_lpn > 6) {
            printf("error! last_lpn - first_lpn > 6\n");
            exit(1);
         }   
      }
      cost += extra_reads*(7*ssd->parameter->time_characteristics.tWC + data_transfer_size*ssd->parameter->subpage_capacity*ssd->parameter->time_characteristics.tRC + ssd->parameter->time_characteristics.tR); //average latency is increased by this amount due to extra sub_requests
      if (cost < 0)
         return 1;
      else 
         return 0;
   } else if (flag1) { 
      return 1;
   } else {//if there is no GC, we don't do anything
      return 0;
   }
}


//distribute read request
struct ssd_info *distribute_modified(struct ssd_info *ssd) 
{
   //unsigned int start, end, first_lsn,last_lsn,lpn,flag=0,flag_attached=0,full_page;
   unsigned int start, end, first_lsn,last_lsn,lpn,flag=0,flag_attached=0,full_page, first_lpn, last_lpn, first_sn, last_sn, sn;
   unsigned int j, k, sub_size;
   int i=0;
   struct request *req;
   struct sub_request *sub;
   int* complt;

   //printf("enter distribute, current time: %lld\n", ssd->current_time);
   full_page=~(0xffffffff<<ssd->parameter->subpage_page);

   req = ssd->request_tail;
   if(req->response_time != 0){
      return ssd;
   }
   if (req->operation==WRITE)
      {
         return ssd;
      }

   unsigned int sub_locations[8][8] = {0}; //hardcoded for setting 8 channels, 8 chips, to record sub_requests locations (for statistics)
   
   if(req != NULL)
      {
         if(req->distri_flag == 0)
            {
               if(req->complete_lsn_count != ssd->request_tail->size)
                  {     
                     first_lsn = req->lsn;            
                     last_lsn = first_lsn + req->size; //1 larger than actual last_lsn. this is used to calculate # of stripes.

                     first_lpn = req->lsn/ssd->parameter->subpage_page;
                     last_lpn = (req->lsn+req->size-1)/ssd->parameter->subpage_page;

                     first_sn = req->lsn/(ssd->parameter->subpage_page*(ssd->parameter->channel_number-1));
                     last_sn = (req->lsn + req->size - 1)/(ssd->parameter->subpage_page*(ssd->parameter->channel_number-1));

                     complt = (int *)req->need_distr_flag;
                     //start = first_lsn - first_lsn % ssd->parameter->subpage_page;
                     start = first_lsn - first_lsn%(ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1)); 
                     //end = (last_lsn/ssd->parameter->subpage_page + 1)*ssd->parameter->subpage_page;
                     //end =  last_lsn%(ssd->parameter->subpage_page) != 0 ? (last_lsn/ssd->parameter->subpage_page + 1)*ssd->parameter->subpage_page : last_lsn;
                     end = last_lsn%(ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1)) != 0 ? (last_lsn/(ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1)) + 1)*ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1) : last_lsn;        
                     //i = (end - start)/32; 
                     unsigned index_length = 2*(end - start)/(ssd->parameter->subpage_page*(ssd->parameter->channel_number - 1)); //i.e., (end - start)/56, (# of stripes)*2 

                     //first_sn & last_sn
                     unsigned int first_state = calc_state(ssd, (char *)&complt[0], (char *)&complt[1], first_lpn, last_lpn, first_sn, last_sn, 1);
                     unsigned int last_state = calc_state(ssd, (char *)&complt[index_length - 2], (char *)&complt[index_length - 1], first_lpn, last_lpn, first_sn, last_sn, 0);       
                     unsigned int first_size = first_lpn%7 == 6 ? transfer_size(ssd, first_state, first_lpn, req) : transfer_size(ssd, first_state, last_lpn - first_lpn + 1 > 1 ? first_lpn + 1 : first_lpn, req);
                     unsigned int last_size = last_lpn%7 == 0 ? transfer_size(ssd, last_state, last_lpn, req) : transfer_size(ssd, last_state, last_lpn - first_lpn + 1 > 1 ? last_lpn - 1 : last_lpn, req);
                     int first_conv = 0, last_conv = 0;
                     if (first_sn != last_sn) {
                        first_conv = convert_to_seq(ssd, first_lpn, last_lpn, first_sn, first_size, 1, req);  
                        last_conv = convert_to_seq(ssd, first_lpn, last_lpn, last_sn, last_size, 2, req);     
                     } else {
                        first_conv = convert_to_seq(ssd, first_lpn, last_lpn, first_sn, first_size, 3, req);  
                        last_conv = convert_to_seq(ssd, first_lpn, last_lpn, last_sn, last_size, 3, req);     
                     }
                     //printf("start: %d, end: %d, i: %d, first_sn_state: %x, last_sn_state: %x\n", start, end, i, first_sn_state, last_sn_state);

                     i = index_length;
                     //while(i >= 0)
                     while(i > 0) //since i represents the # of int we have to check 
                        {  
                           for(j=0; j<32/ssd->parameter->subpage_page; j++)
                              {
                                 k = (complt[index_length-i] >> (ssd->parameter->subpage_page*j)) & full_page;
                     
                                 if ((index_length - i) % 2 == 1 && j == 3)
                                    assert(k == 0);

                                 lpn = start/ssd->parameter->subpage_page + (index_length - i)*32/ssd->parameter->subpage_page + j - (index_length - i)/2; // since 2nd int only stores states of 3 pages, when we calculate lpn we have to minus a page whenever (index_length - i)%2 is 1 (i.e., 0, 1, 2, 3, 1 and 3 just stores states of 3 pages)
                                 if ((index_length - i) % 2 == 1 && j == 3)
                                    lpn = lpn - 1;
                                 sn = lpn/(ssd->parameter->channel_number - 1); 
                                 //printf("index_length - i: %d, j: %d, lpn: %d, k: %x, first_lpn: %d, last_lpn: %d\n", index_length - i, j, lpn, k, first_lpn, last_lpn);
            
                                 //if (k!=0)
                                 if (k != 0 && first_lpn <= lpn && lpn <= last_lpn) {               

                                    sub_size=transfer_size(ssd,k,lpn,req);    
                                    if (sub_size==0) 
                                       {
                                          continue;
                                       }
                                    else
                                       {
                                          //sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation);
                                          if (ssd->parameter->adaptive_read) {
                                             if (sn != first_sn && sn != last_sn) {
                                                sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation,0,1);
                                             } else if (sn != last_sn) {
                                                sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation,0,first_conv);
                                             } else {
                                                sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation,0,last_conv);
                                             }
                                          } else {
                                             sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation,0,0); //which means there is no parity read and we can't drop any sub_request
                                          }
                                          //printf("distribute_modified/creat_sub_request(read), lpn: %d, sub_size: %d\n", lpn, sub_size);
                 
                                          struct local *loc = find_location(ssd, ssd->dram->map->map_entry[lpn].pn);
                                          sub_locations[loc->channel][loc->chip] = 1; //update sub_requests location info
                                       }
                
                                 } else if (lpn < first_lpn || lpn > last_lpn || ((index_length - i) % 2 == 1 && j == 3)) {
                               
                                    //printf("index_length: %d, i: %d, j: %d\n", index_length, i, j);
                                    //convert random read to sequential read
                                    if (((sn == first_sn && first_conv) || (sn == last_sn && last_conv) || (sn > first_sn && sn < last_sn)) && ssd->parameter->adaptive_read) {

                                       if(!((index_length - i) % 2 == 1 && j == 3)) {//last page of 2nd int correspond to parity page
                                          if (lpn < first_lpn)
                                             sub_size = first_size;
                                          else
                                             sub_size = last_size;

                                          //we have to check whether this page has been written before
                                          if (ssd->dram->map->map_entry[lpn].state != 0) 
                                             sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation,0,1);
                                          //printf("distribute_modified/creat_sub_request(extra read), lpn: %d, sub_size: %d\n", lpn, sub_size);
                 
                                       } else {
                                          //since we are reading sequentially, most of the time parity page size is 8. rare case happens when read size < page and we are padding to make it sequential
                                          sub_size = 8;
                                          if (sn == first_sn)
                                             sub_size = first_size;
                                          else if (sn == last_sn)
                                             sub_size = last_size;

                                          //parity page should have been already allocated
                                          if (ssd->dram->map->parity_entry[sn].state == 0) {
                                             printf("error! ssd->dram->map->parity_entry[(lpn - 7)/7].state = 0\n");
                                             exit(1);
                                          }
                    
                                          sub=creat_sub_request(ssd,sn,sub_size,0,req,req->operation,1,1);
                                          //printf("distribute_modified/creat_sub_request(parity read), sn: %d, sub_size: %d\n", sn, sub_size);
                                       }

                                    }
                                 }
                              }
                           i--;
                        }
         
                  }
               else
                  {
                     //if read req is not multiple of full page but all hit in the buffer, complete_lsn and size is checked
                     req->begin_time=ssd->current_time;
                     req->response_time=ssd->current_time+1000;         
                  }

            }
      }
            
   return ssd;
}
