  using System;
  using System.Collections;
  using System.Collections.Generic;
  using System.Drawing;
  using System.Drawing.Imaging;
  using System.IO;
  using System.Runtime.InteropServices;
  using OpenCL.Net;




namespace Project1
{

    public class Computations
    {


        public struct clState 
{
             public  Context cl_context;
            //cl_context OpenCL. context;
            public Kernel cl_kernel;
            public CommandQueue cl_command_queue;
            public Program cl_program;
            public Mem outputBuffer;
            public uint4 CLBuffer0;

            public uint4 padbuffer8;


            public IntPtr padbufsize;
            //	size_t padbufsize;
            public IntPtr cldata;
            //	void * cldata;
            public 	bool hasBitAlign;
            public 	bool hasOpenCL11plus;
            public 	bool hasOpenCL12plus;
            public 	bool goffset;
            public uint16 vwidth;
            //	uint16 vwidth;
            public IntPtr max_work_size;
            public IntPtr wsize;

} 


public struct dev_blk_ctx {
public OpenCL.Net.uint16 	 ctx_a;
  public   uint16  ctx_b;
   public uint16 ctx_c; 
  public  uint16 ctx_d;
   public uint16 ctx_e; uint16 ctx_f; uint16 ctx_g; uint16 ctx_h;

public	uint16 cty_a; uint16 cty_b; uint16 cty_c; uint16 cty_d;
public	uint16 cty_e; uint16 cty_f; uint16 cty_g; uint16 cty_h;
public	uint16 merkle; uint16 ntime; uint16 nbits; uint16 nonce;
public	uint16 fW0; uint16 fW1; uint16 fW2; uint16 fW3; uint16 fW15;
public	uint16 fW01r; uint16 fcty_e; uint16 fcty_e2;
public	uint16 W16; uint16 W17; uint16 W2;
public	uint16 PreVal4; uint16 T1;
	public uint16 C1addK5; uint16 D1A; uint16 W2A; uint16 W17_2;
public	uint16 PreVal4addT1; uint16 T1substate0;
public	uint16 PreVal4_2;
public	uint16 PreVal0;
public	uint16 PreW18;
public	uint16 PreW19;
public	uint16 PreW31;
public	uint16 PreW32;
    public work work;
	/* For diakgcn */
public	uint16 B1addK6, PreVal0addK7, W16addK16, W17addK17;
public	uint16 zeroA, zeroB;
public	uint16 oneA, twoA, threeA, fourA, fiveA, sixA, sevenA;
} 

public struct work 
{
    public int longnonce;

    public OpenCL.Net.uchar16 midstate;
    public OpenCL.Net.uint4 midstate0;

    public OpenCL.Net.uint4 midstate16;


    public int device_target;
    public IntPtr data;


}





        private Context _context;
        private Device _device;

        private void CheckErr(ErrorCode err, string name)
        {
            if (err != ErrorCode.Success)
            {
                Console.WriteLine("ERROR: " + name + " (" + err.ToString() + ")");
            }
        }

        private void ContextNotify(string errInfo, byte[] data, IntPtr cb, IntPtr userData)
        {
            Console.WriteLine("OpenCL Notification: " + errInfo);
        }



        public void Setup()
        {
            ErrorCode error;
            Platform[] platforms = Cl.GetPlatformIDs(out error);
            List<Device> devicesList = new List<Device>();

            CheckErr(error, "Cl.GetPlatformIDs");

            foreach (Platform platform in platforms)
            {
                //ToDO:Log Platform Device Name;
                //		status = clGetPlatformInfo(platform, CL_PLATFORM_NAME, sizeof(pbuff), pbuff, NULL);
                //		status = clGetPlatformInfo(platform, CL_PLATFORM_VERSION, sizeof(pbuff), pbuff, NULL);
                //		status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, NULL, &numDevices);
                //ToDO: Saved the compiled version so it can be built the next time as a binary:
                //ToDO:  Enable certain cards, fan control, scheduling

                string platformName = Cl.GetPlatformInfo(platform, PlatformInfo.Name, out error).ToString();
                Console.WriteLine("Platform: " + platformName);
                CheckErr(error, "Cl.GetPlatformInfo");

                //We will be looking only for GPU devices
                foreach (OpenCL.Net.Device device in Cl.GetDeviceIDs(platform, DeviceType.Gpu, out error))
                {
                    CheckErr(error, "Cl.GetDeviceIDs");
                    Console.WriteLine("Device: " + device.ToString());
                    devicesList.Add(device);
                }
            }

            if (devicesList.Count <= 0)
            {
                Console.WriteLine("No devices found.");
                return;
            }

            _device = devicesList[0];

            if (Cl.GetDeviceInfo(_device, OpenCL.Net.DeviceInfo.ImageSupport, out error).CastTo<OpenCL.Net.Bool>() == OpenCL.Net.Bool.False)
            {
                Console.WriteLine("No image support.");
                return;
            }

            _context = Cl.CreateContext(null, 1, new[] { _device }, ContextNotify, IntPtr.Zero, out error); //Second parameter is amount of devices
            CheckErr(error, "Cl.CreateContext");
        }

        public void ImagingTest(string inputImagePath, string outputImagePath)
        {
            ErrorCode error;

            //Load and compile kernel source code.
            string programPath = System.Environment.CurrentDirectory + "/../../imagingtest.cl";  //The path to the source file may vary

            if (!System.IO.File.Exists(programPath))
            {
                Console.WriteLine("Program doesn't exist at path " + programPath);
                return;
            }

            string programSource = System.IO.File.ReadAllText(programPath);

            using (Program program = Cl.CreateProgramWithSource(_context, 1, new[] { programSource }, null, out error))
            {
                CheckErr(error, "Cl.CreateProgramWithSource");

                //Compile kernel source
                error = Cl.BuildProgram(program, 1, new[] { _device }, string.Empty, null, IntPtr.Zero);
                CheckErr(error, "Cl.BuildProgram");

                //Check for any compilation errors
                if (Cl.GetProgramBuildInfo(program, _device, ProgramBuildInfo.Status, out error).CastTo<BuildStatus>()
                    != BuildStatus.Success && 1==0)
                {
                    CheckErr(error, "Cl.GetProgramBuildInfo");
                    Console.WriteLine("Cl.GetProgramBuildInfo != Success");
                    Console.WriteLine(Cl.GetProgramBuildInfo(program, _device, ProgramBuildInfo.Log, out error));
                    return;
                }

                //Create the required kernel (entry function)
                Kernel kernel = Cl.CreateKernel(program, "imagingTest", out error);
                CheckErr(error, "Cl.CreateKernel");

                int intPtrSize = 0;
                intPtrSize = Marshal.SizeOf(typeof(IntPtr));

                //Image's RGBA data converted to an unmanaged[] array
                byte[] inputByteArray;
                //OpenCL memory buffer that will keep our image's byte[] data.
                Mem inputImage2DBuffer;

                OpenCL.Net.ImageFormat clImageFormat = new OpenCL.Net.ImageFormat(ChannelOrder.RGBA, ChannelType.Unsigned_Int8);
                
                int inputImgWidth, inputImgHeight;

                int inputImgBytesSize;

                int inputImgStride;

                //Try loading the input image
                using (FileStream imageFileStream = new FileStream(inputImagePath, FileMode.Open))
                {
                    System.Drawing.Image inputImage = System.Drawing.Image.FromStream(imageFileStream);

                    if (inputImage == null)
                    {
                        Console.WriteLine("Unable to load input image");
                        return;
                    }

                    inputImgWidth = inputImage.Width;
                    inputImgHeight = inputImage.Height;

                    System.Drawing.Bitmap bmpImage = new System.Drawing.Bitmap(inputImage);

                    //Get raw pixel data of the bitmap
                    //The format should match the format of clImageFormat
                    BitmapData bitmapData = bmpImage.LockBits(new Rectangle(0, 0, bmpImage.Width, bmpImage.Height),
                                                              ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);//inputImage.PixelFormat);

                    inputImgStride = bitmapData.Stride;
                    inputImgBytesSize = bitmapData.Stride * bitmapData.Height;

                    //Copy the raw bitmap data to an unmanaged byte[] array
                    inputByteArray = new byte[inputImgBytesSize];
                    Marshal.Copy(bitmapData.Scan0, inputByteArray, 0, inputImgBytesSize);

                    //Allocate OpenCL image memory buffer
                    inputImage2DBuffer = (OpenCL.Net.Mem)OpenCL.Net.Cl.CreateImage2D(_context,
                        OpenCL.Net.MemFlags.CopyHostPtr | OpenCL.Net.MemFlags.ReadOnly, clImageFormat,
                                                     (IntPtr)bitmapData.Width, (IntPtr)bitmapData.Height,
                                                        (IntPtr)0, inputByteArray, out error);
                    CheckErr(error, "Cl.CreateImage2D input");
                }

                //Unmanaged output image's raw RGBA byte[] array
                byte[] outputByteArray = new byte[inputImgBytesSize];

                //Allocate OpenCL image memory buffer
                OpenCL.Net.Mem outputImage2DBuffer = (OpenCL.Net.Mem)OpenCL.Net.Cl.CreateImage2D(_context, OpenCL.Net.MemFlags.CopyHostPtr | OpenCL.Net.MemFlags.WriteOnly, clImageFormat,
                                                               (IntPtr)inputImgWidth, (IntPtr)inputImgHeight, (IntPtr)0, outputByteArray, out error);
                
                CheckErr(error, "Cl.CreateImage2D output");

                //Pass the memory buffers to our kernel function
                error = Cl.SetKernelArg(kernel, 0, (IntPtr)intPtrSize, inputImage2DBuffer);
                error |= Cl.SetKernelArg(kernel, 1, (IntPtr)intPtrSize, outputImage2DBuffer);
                CheckErr(error, "Cl.SetKernelArg");

                //Create a command queue, where all of the commands for execution will be added
                CommandQueue cmdQueue = Cl.CreateCommandQueue(_context, _device, (CommandQueueProperties)0, out error);
                CheckErr(error, "Cl.CreateCommandQueue");
                
                OpenCL.Net.Event clevent;

                //Copy input image from the host to the GPU.
                IntPtr[] originPtr = new IntPtr[] { (IntPtr)0, (IntPtr)0, (IntPtr)0 };  //x, y, z
                IntPtr[] regionPtr = new IntPtr[] { (IntPtr)inputImgWidth, (IntPtr)inputImgHeight, (IntPtr)1 }; //x, y, z
                IntPtr[] workGroupSizePtr = new IntPtr[] { (IntPtr)inputImgWidth, (IntPtr)inputImgHeight, (IntPtr)1 };
                error = Cl.EnqueueWriteImage(cmdQueue, inputImage2DBuffer, OpenCL.Net.Bool.True, originPtr, regionPtr, (IntPtr)0, (IntPtr)0, inputByteArray, 0, null, out clevent);
                CheckErr(error, "Cl.EnqueueWriteImage");

                //Execute our kernel (OpenCL code)
                //                  CommandQueue q =  new OpenCL.Net.CommandQueue();



                //enqueue nd range kernel

                // error = cmdQueue.EnqueueKernel(cmdQueue, kernel, 2, null, workGroupSizePtr, null, 0, null, out clevent);
                //   OpenCL.Net.Cl.EnqueueNDRangeKernel(
                OpenCL.Net.Cl.EnqueueNDRangeKernel(cmdQueue, kernel, 2, null, workGroupSizePtr, null, 0, null, out clevent);

                CheckErr(error, "Cl.EnqueueNDRangeKernel");

                //Wait for completion of all calculations on the GPU.
                error = Cl.Finish(cmdQueue);
                CheckErr(error, "Cl.Finish");

                //Read the processed image from GPU to raw RGBA data byte[] array
                error = Cl.EnqueueReadImage(cmdQueue, outputImage2DBuffer, OpenCL.Net.Bool.True, originPtr, regionPtr,
                                           (IntPtr)0, (IntPtr)0, outputByteArray, 0, null, out clevent);
                CheckErr(error, "Cl.clEnqueueReadImage");

                //Clean up memory
                Cl.ReleaseKernel(kernel);
                Cl.ReleaseCommandQueue(cmdQueue);

                Cl.ReleaseMemObject(inputImage2DBuffer);
                Cl.ReleaseMemObject(outputImage2DBuffer);

                //Get a pointer to our unmanaged output byte[] array
                GCHandle pinnedOutputArray = GCHandle.Alloc(outputByteArray, GCHandleType.Pinned);
                IntPtr outputBmpPointer = pinnedOutputArray.AddrOfPinnedObject();

                //Create a new bitmap with processed data and save it to a file.
                Bitmap outputBitmap = new Bitmap(inputImgWidth, inputImgHeight, inputImgStride, PixelFormat.Format32bppArgb, outputBmpPointer);

                outputBitmap.Save(outputImagePath, System.Drawing.Imaging.ImageFormat.Png);

                pinnedOutputArray.Free();
            }
        }









        public void ScryptTest()
        {
            ErrorCode error;

            //Load and compile kernel source code.
            string programPath = System.Environment.CurrentDirectory + "/../../scrypt.cl";  //Cl
            if (!System.IO.File.Exists(programPath))
            {                Console.WriteLine("Program doesn't exist at path " + programPath);                return;
            }

            string programSource = System.IO.File.ReadAllText(programPath);
            IntPtr[] sz = new IntPtr[programSource.Length*2];
            Program program =Cl.CreateProgramWithSource(_context, 1, new[] { programSource }, null, out error);
            if (1==1)
            {
                CheckErr(error, "Cl.CreateProgramWithSource");

                //                status = clBuildProgram(clState->program, 1, &devices[gpu], ""-D LOOKUP_GAP=%d -D CONCURRENT_THREADS=%d -D WORKSIZE=%d", NULL, NULL);
                //Compile kernel source
                error = Cl.BuildProgram(program, 1, new[] { _device },"-D LOOKUP_GAP=1 -D CONCURRENT_THREADS=1 -D WORKSIZE=1", null, IntPtr.Zero);
                CheckErr(error, "Cl.BuildProgram");

                //Check for any compilation errors
                if (Cl.GetProgramBuildInfo(program, _device, ProgramBuildInfo.Status, out error).CastTo<BuildStatus>()
                    != BuildStatus.Success && 1 == 0)
                {
                    CheckErr(error, "Cl.GetProgramBuildInfo");
                    Console.WriteLine("Cl.GetProgramBuildInfo != Success");
                    Console.WriteLine(Cl.GetProgramBuildInfo(program, _device, ProgramBuildInfo.Log, out error));
                    return;
                }

                //Create the required kernel (entry function) [search]
                Kernel kernel = Cl.CreateKernel(program, "search", out error);
                
                CheckErr(error, "Cl.CreateKernel");
                int intPtrSize = 0;
                intPtrSize = Marshal.SizeOf(typeof(IntPtr));

                //Image's RGBA data converted to an unmanaged[] array
                byte[] inputByteArray;
                //OpenCL memory buffer that will keep our image's byte[] data.
                Mem inputImage2DBuffer;
                //Create a command queue, where all of the commands for execution will be added
                CommandQueue cmdQueue = Cl.CreateCommandQueue(_context, _device, (CommandQueueProperties)0, out error);
                CheckErr(error, "Cl.CreateCommandQueue");
                clState _clState = new clState();
                _clState.cl_command_queue = cmdQueue;
                _clState.cl_kernel = kernel;
                _clState.cl_context = _context;


                IntPtr buffersize = new IntPtr(1024);
                IntPtr blank_res = new IntPtr(1024);
                Object thrdataRes = new Object();

                
                //int buffersize = 1024;
                OpenCL.Net.Event clevent;
                 //                 status |= clEnqueueWriteBuffer(clState->commandQueue, clState->outputBuffer, CL_TRUE, 0,  buffersize, blank_res, 0, NULL, NULL);
                dev_blk_ctx blk = new dev_blk_ctx();
                ErrorCode err =  queue_scrypt_kernel(_clState,blk);



                ErrorCode status = Cl.EnqueueWriteBuffer(_clState.cl_command_queue,
                    _clState.outputBuffer, OpenCL.Net.Bool.True, new IntPtr(0), buffersize, blank_res, 0, null, out clevent);

                IntPtr[] globalThreads = new IntPtr[0];
                IntPtr[] localThreads = new IntPtr[0];

                //uint16 workdim = new uint16(1);
                uint workdim = 1;

                status = Cl.EnqueueNDRangeKernel(_clState.cl_command_queue,_clState.cl_kernel, workdim, null, globalThreads, localThreads, 0, null, out clevent);
                CheckErr(error, "Cl.EnqueueNDRangeKernel");

                IntPtr offset = new IntPtr(0);

                status = Cl.EnqueueReadBuffer(_clState.cl_command_queue,_clState.outputBuffer,OpenCL.Net.Bool.False,offset,buffersize,thrdataRes, 0,null, out clevent);

                //Wait for completion of all calculations on the GPU.
                error = Cl.Finish(_clState.cl_command_queue);

                CheckErr(error, "Cl.Finish");

                //Clean up memory
                Cl.ReleaseKernel(_clState.cl_kernel);
                Cl.ReleaseCommandQueue(_clState.cl_command_queue);

            }


        }




ErrorCode queue_scrypt_kernel(clState _clState, dev_blk_ctx blk)
{

    //Scantime
    

    blk.work.midstate0 = new uint4(0);

    blk.work.midstate16 = new uint4(1);



   // unsigned char *midstate = blk->work->midstate;
	int  num = 0;
	uint16 le_target;
  le_target = new uint16(Convert.ToUInt16(blk.work.device_target + 28));
    _clState.cldata = blk.work.data;
    _clState.CLBuffer0 = new uint4(0);

    _clState.outputBuffer = new Mem();

    _clState.padbuffer8 = new uint4(0);


    OpenCL.Net.Event clevent;
    OpenCL.Net.ImageFormat clImageFormat = new OpenCL.Net.ImageFormat(ChannelOrder.RGBA, ChannelType.Unsigned_Int8);
    ErrorCode err1;
    byte[] inputByteArray = new byte[1024];

              
    //buffer0 = (OpenCL.Net.Mem)OpenCL.Net.Cl.CreateImage2D(_clState.cl_context,                 OpenCL.Net.MemFlags.CopyHostPtr | OpenCL.Net.MemFlags.ReadOnly, clImageFormat,                                              (IntPtr)1024, (IntPtr)1,                                                  (IntPtr)0, inputByteArray, out err1);
   // buffer0 = (OpenCL.Net.Mem)inputByteArray;
   // byte[] byteSrcImage2DData = new byte[srcIMGBytesSize];
    Mem buffer0 = (Mem)OpenCL.Net.Cl.CreateBuffer(_clState.cl_context, MemFlags.ReadWrite, 1024,out err1);

              
	ErrorCode status =Cl.EnqueueWriteBuffer(_clState.cl_command_queue, buffer0, OpenCL.Net.Bool.True, (IntPtr)0, (IntPtr)1024, _clState.cldata, 0, null,out clevent);

    //
//      status = OpenCL.Net.Cl.SetKernelArg(_clState.Kernel,  num++, sizeof(var), (void *)&var)

  // CL_SET_VARG(args, var) status |= clSetKernelArg(*kernel, num++, args * sizeof(uint), (void *)var)



    int intPtrSize = 0;
    intPtrSize = Marshal.SizeOf(typeof(IntPtr));



    OpenCL.Net.Cl.SetKernelArg(_clState.cl_kernel, 0,(IntPtr)intPtrSize, _clState.CLBuffer0);


    OpenCL.Net.Cl.SetKernelArg(_clState.cl_kernel, 1, _clState.outputBuffer);
    OpenCL.Net.Cl.SetKernelArg(_clState.cl_kernel, 2, _clState.padbuffer8);
    OpenCL.Net.Cl.SetKernelArg(_clState.cl_kernel, 3, blk.work.midstate0);
    OpenCL.Net.Cl.SetKernelArg(_clState.cl_kernel, 4, blk.work.midstate16);
    //   CL_SET_VARG(4, &midstate[0]);
    //	CL_SET_VARG(4, &midstate[16]);
    OpenCL.Net.Cl.SetKernelArg(_clState.cl_kernel, 5,le_target);
    //error = Cl.SetKernelArg(kernel, 0, (IntPtr)intPtrSize, inputImage2DBuffer);
    CheckErr(status, "Cl.SetKernelArg");
return status;
}



















    }
















}

