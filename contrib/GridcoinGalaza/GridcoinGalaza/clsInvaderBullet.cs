//   
// Purpose:      Enemy bullets
// Rev:          1.0
// Last updated: 22/03/10

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;

namespace GridcoinGalaza
{
    public class clsInvaderBullet : clsCommon
    {

        private System.Random objRandom = new System.Random(Convert.ToInt32(System.DateTime.Now.Ticks % System.Int32.MaxValue));
 
        // Obj refs and instances
        private System.Drawing.Bitmap bullet;
        private ImageAttributes ImagingAtt = new ImageAttributes();

        //------------------------------------------------------------------------------------------------------------------
        // Purpose: Class constructor  
        //------------------------------------------------------------------------------------------------------------------

        // Call to super / base class
        public clsInvaderBullet(int x, int y): base(x, y, 12, 32)
        {

            // Load resource image(s) & remove background and thu a sprite is born
            bullet = GridcoinGalaza.Properties.Resources.enemyBullet;
            bullet.MakeTransparent(Color.White);

        }

        private int getRandomNumber(int Low, int High)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Returns a random number, between the optional Low and High parameters
            //------------------------------------------------------------------------------------------------------------------

            return objRandom.Next(Low, High + 1);
        }
        public void moveBullets(Graphics Destination, int type)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to move the invaders bullets by 5 pixels every frame   
            //------------------------------------------------------------------------------------------------------------------
            int d = getRandomNumber(6, 12);
            base.setY(base.getY() + d);

            //
            int c = getRandomNumber(0, 3);
            base.setX(base.getX() + (c - 1));

            if (type == 2)
            {
                base.setX(base.getX() + (c + 1));

            }
            // Sync collsion rect
            base.setRectX(base.getX() + 2);
            base.setRectY(base.getY() + 2);
            base.setRectW(base.getW() - 5);
            base.setRectH(base.getH() - 5);
            // Render them
            this.Draw(Destination);

        }
        


        private void Draw(Graphics Destination)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to render the invader's bullets 
            //------------------------------------------------------------------------------------------------------------------

            // Draw sprite
            Destination.DrawImage(bullet, new Rectangle(base.getX(), base.getY(), base.getW(), base.getH()), 0, 0, base.getW(), base.getH(), GraphicsUnit.Pixel, ImagingAtt);

        }

    }

}
