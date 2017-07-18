Imports System.Speech.Synthesis
Imports System.Collections.ObjectModel
Public Class SpeechSynthesis
    Private Function DayInterval() As String
        Dim iHour As Integer = Hour(Now)
        Dim sInterval As String = ""
        If iHour > 0 And iHour <= 12 Then sInterval = "Morning"
        If iHour >= 12 And iHour < 18 Then sInterval = "Afternoon"
        If iHour >= 18 And iHour < 21 Then sInterval = "Evening"
        If iHour >= 21 And iHour <= 23 Then sInterval = "Night"
        Return sInterval
    End Function
    Public Function AddressUserBySurname(Magnitude As Long) As Boolean
        Dim sSentence As String = ""
        Try

            Dim sName As String

            sName = KeyValue("Name")
            If Len(sName) = 0 Then
                sName = KeyValue("Email")
                If Len(sName) > 0 Then
                    Dim vName() As String
                    vName = Split(sName, "@")
                    If UBound(vName) > 0 Then
                        sName = vName(0)
                    End If
                End If
            End If
            If Len(sName) = 0 Then sName = "Investor"
            Dim sSurname = KeyValue("Surname")
            Dim sDayInterval As String = DayInterval()
            Dim sMag As String = "0"
            Try
                sMag = "0" + Magnitude.ToString()
            Catch ex As Exception
            End Try
            sSentence = "Good " + sDayInterval + " " + sSurname + " " + sName + ", Your Magnitude is " + sMag + "."
            If mbTestNet Then sSentence += ",in Test Net." Else mbTestNet += ",in Production."
            Speak(sSentence)
            Return True
        Catch ex As Exception
            Log("Error while Speaking " + Trim(sSentence) + ex.Message)
        End Try

    End Function
    Public Function Speak(sSentence As String) As Boolean
        Try
            If KeyValue("enablespeech") <> "true" Then
                Log("Speech disabled.")
                Exit Function
            End If
            Dim synth As New SpeechSynthesizer()
            synth.SetOutputToDefaultAudioDevice()
            Dim cInstalledVoice As ReadOnlyCollection(Of System.Speech.Synthesis.InstalledVoice) = synth.GetInstalledVoices()
            If cInstalledVoice.Count > 0 Then
                Dim sVoiceName As String = cInstalledVoice(0).VoiceInfo.Name
                synth.Speak(sSentence)
            End If
            synth = Nothing
            Return True
        Catch ex As Exception
            Log("Error while speaking " + sSentence + ": " + ex.Message)
        End Try
        Return False
    End Function
End Class
