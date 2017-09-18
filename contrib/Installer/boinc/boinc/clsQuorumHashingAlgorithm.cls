Public Class clsQuorumHashingAlgorithm

    Private MagnitudeBreaks As String = "0-25,25-500,500-1000,1000-10000,10000-50000,50000-100000,100000-999999,1000000-Inf"
    Private DitherConstants As String = ".8,.2,.1,.025,.006,.003,.0015,.0007"
    Private vMagBreaks() As String
    Private vDitherConstants() As String
    Public Sub New()
        vMagBreaks = Split(MagnitudeBreaks, ",")
        vDitherConstants = Split(DitherConstants, ",")
    End Sub
    Public Function GetDitherMag(Data As Double) As Double
        Dim Dither As Double = 0.1
        For x = 0 To UBound(vMagBreaks)
            Dim vBreak() As String = Split(vMagBreaks(x), "-")
            Dim dLowBreak As Double = Val(vBreak(0))
            Dim dHighBreak As Double = 0
            If vBreak(1) = "Inf" Then dHighBreak = dLowBreak * 10 Else dHighBreak = Val(vBreak(1))
            If Data >= dLowBreak And Data <= dHighBreak Then
                Dither = Val(vDitherConstants(x))
                Return Dither
            End If
        Next
        Return Dither
        'This function is used by the neural network to snap a magnitude to the grid - so all nodes agree on the magnitude
    End Function

    Public Function QuorumHashingAlgorithm(data As String)
        Dim sMags As String
        sMags = ExtractXML(data, "<MAGNITUDES>")
        Dim vMags() As String
        vMags = Split(sMags, ";")
        Dim sHashIn As String = ""
        For x As Integer = 0 To UBound(vMags)
            If Len(vMags(x)) > 10 Then
                Dim vRow() As String = Split(vMags(x), ",")
                If UBound(vRow) > 0 Then
                    If Len(vRow(0)) > 5 Then
                        Dim sCPID As String = vRow(0)
                        Dim dMag = Math.Round(Val(vRow(1)), 0, MidpointRounding.AwayFromZero)
                        Dim sRow = sCPID + "," + Trim(dMag)
                        If KeyValue("DEBUG_QHA") = "TRUE" Then Log(sRow)
                        sHashIn += CPIDHash(dMag, sCPID) + "<COL>"
                    End If
                End If
            End If
        Next x
        Dim sHash As String = GetMd5String(sHashIn)

        If KeyValue("DEBUG_QHA") = "TRUE" Then
            Log(sHashIn)
            Log(sHash)
        End If
        Return sHash
    End Function
    Private Function CPIDHash(dMagIn As Double, sCPID As String) As String
        Dim sMag As String = Trim(Math.Round(dMagIn, 0, MidpointRounding.AwayFromZero))
        Dim dMagLength As Double = Len(sMag)
        Dim dExponent As Double = Math.Pow(dMagLength, 5)
        Dim sMagComponent1 As String = Trim(Math.Round(dMagIn / (dExponent + 0.01), 0, MidpointRounding.AwayFromZero))
        Dim sSuffix = Trim(Math.Round(dMagLength * dExponent, 0, MidpointRounding.AwayFromZero))
        Dim sHash As String = sCPID + sMagComponent1 + sSuffix
        Return sHash
    End Function
End Class
