Option Strict Off
Friend Structure ColorRGB
    Public R As Short
	Public G As Short
	Public B As Short
End Structure

Friend Module modGRCWireFrameColor
    Public Function ColorSet(ByVal R As Short, ByVal G As Short, ByVal B As Short) As ColorRGB
        ColorSet.R = R
        ColorSet.G = G
        ColorSet.B = B
    End Function
    Public Function ColorAdd(ByRef C1 As ColorRGB, ByRef C2 As ColorRGB) As ColorRGB
        ColorAdd.R = C1.R + C2.R
        ColorAdd.G = C1.G + C2.G
        ColorAdd.B = C1.B + C2.B
    End Function
    Public Function ColorSub(ByRef C1 As ColorRGB, ByRef C2 As ColorRGB) As ColorRGB
        ColorSub.R = C1.R - C2.R
        ColorSub.G = C1.G - C2.G
        ColorSub.B = C1.B - C2.B
    End Function
    Public Function ColorScale(ByRef c As ColorRGB, ByVal S As Single) As ColorRGB
        ColorScale.R = c.R * S
        ColorScale.G = c.G * S
        ColorScale.B = c.B * S
    End Function
    Public Function ColorPlus(ByRef c As ColorRGB, ByVal L As Short) As ColorRGB
        ColorPlus.R = c.R + L
        ColorPlus.G = c.G + L
        ColorPlus.B = c.B + L
    End Function
    Public Function ColorInvert(ByRef c As ColorRGB) As ColorRGB
        ColorInvert.R = 255 - c.R
        ColorInvert.G = 255 - c.G
        ColorInvert.B = 255 - c.B
    End Function
    Public Function ColorGray(ByVal R As Short, ByVal G As Short, ByVal B As Short) As ColorRGB

        Dim Temp As Short = (R + G + B) / 3

        ColorGray.R = Temp
        ColorGray.G = Temp
        ColorGray.B = Temp

    End Function

    Public Function ColorRandom() As ColorRGB

        Randomize()
        ColorRandom.R = Rnd() * 255
        ColorRandom.G = Rnd() * 255
        ColorRandom.B = Rnd() * 255

    End Function
    Public Function ColorLongToRGB(ByVal lC As Integer) As ColorRGB

        ColorLongToRGB.R = (lC And &HFF%)
        ColorLongToRGB.G = (lC And &HFF00%) / &H100%
        ColorLongToRGB.B = (lC And &HFF0000) / &H10000

    End Function

    Public Function ColorRGBToLong(ByRef c As ColorRGB) As Integer
        Return RGB(c.R, c.G, c.B)
    End Function

End Module
