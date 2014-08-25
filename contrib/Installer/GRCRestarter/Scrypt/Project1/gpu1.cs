using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Drawing.Drawing2D;

namespace Project1
{
    public partial class RamGec_About_Box1 : Form
    {
       
        public RamGec_About_Box1()
        {
            InitializeComponent();
        }

        public RamGec_About_Box1(string TopCaption, string Link)
        {
            InitializeComponent();
           
        }

        private void okButton_Click(object sender, EventArgs e)
        {
            Close();
        }

       
        public static void Main()
        {
            RamGec_About_Box1 rg = new RamGec_About_Box1();
            rg.RamGec_About_Box1_Load(null, null);

        }
        private void RamGec_About_Box1_Load(object sender, EventArgs e)
        {
            Computations c = new Computations();

            c.Setup();
        //    c.ImagingTest("c:\\splashbmp.bmp", "c:\\splashg.bmp");
            c.ScryptTest();




        }

    }
}
