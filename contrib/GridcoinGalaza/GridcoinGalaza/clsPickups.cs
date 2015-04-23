//   - 
// Purpose:      Player pickup items
// Rev:          1.0
// Last updated: 22/03/10

using System;
using System.Drawing;
using System.Drawing.Imaging;

namespace GridcoinGalaza
{
    public class clsPickups : clsCommon
    {

        // Obj refs and instances
        private System.Drawing.Bitmap pickup;
        private ImageAttributes ImagingAtt = new ImageAttributes();

        // Properties for this class 
        private int pickupType;

        public clsPickups(int x, int y, int pickupType): base(x, y, 0, 0)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Class constructor  
            //------------------------------------------------------------------------------------------------------------------

            //:: Load resource image(s) & remove background and thu a sprite is born ::
            switch (pickupType)
            {
                case 0:
                    pickup = GridcoinGalaza.Properties.Resources.pickupA;
                    break;
                case 1:
                    pickup = GridcoinGalaza.Properties.Resources.pickupB;
                    break;
                case 2:
                    pickup = GridcoinGalaza.Properties.Resources.pickupC;
                    break;
            }

            // Remove background
            pickup.MakeTransparent(Color.Black);

            // Assign attributes this class
            this.pickupType = pickupType;

            // Apply width and height call to super
            base.setH(pickup.Height);
            base.setW(pickup.Width);
        }

        public void setPickupType(int pickupType)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set pickup type ref num)   
            //------------------------------------------------------------------------------------------------------------------

            this.pickupType = pickupType;
        }

        public int getPickupType()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch pickup type ref num)  
            //------------------------------------------------------------------------------------------------------------------

            return this.pickupType;
        }

        public void movePickups(Graphics Destination)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to move the pickups by 2 pixels every frame  
            //------------------------------------------------------------------------------------------------------------------

            // Scroll pickups
            base.setY(base.getY() + 2);

            // Sync collision rect
            base.setRectX(base.getX());
            base.setRectY(base.getY());
            base.setRectW(base.getW());
            base.setRectH(base.getH());

            // Invoke draw method
            this.Draw(Destination);
        }

        private void Draw(Graphics Destination)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to render the pickup 
            //------------------------------------------------------------------------------------------------------------------

            // Draw sprite
            Destination.DrawImage(pickup, new Rectangle(base.getX(), base.getY(), base.getW(), base.getH()), 0, 0, base.getW(), base.getH(), GraphicsUnit.Pixel, ImagingAtt);

        }
    }
}
