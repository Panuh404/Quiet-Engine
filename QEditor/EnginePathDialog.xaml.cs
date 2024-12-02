using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using Path = System.IO.Path;

namespace QEditor
{
    public partial class EnginePathDialog : Window
    {
        public string QuietPath { get; private set; }


        public EnginePathDialog()
        {
            InitializeComponent();
            Owner = Application.Current.MainWindow;
        }

        private void OnOk_Button_Click(object sender, RoutedEventArgs e)
        {
            var path = pathTextBox.Text.Trim();
            messageTextBlock.Text = string.Empty;

            if (string.IsNullOrEmpty(path))
            {
                messageTextBlock.Text = "Invalid Path";
            }
            else if (path.IndexOfAny(Path.GetInvalidPathChars()) != -1)
            {
                messageTextBlock.Text = "Invalid Character(s) used in Path";
            }
            else if (!Directory.Exists(Path.Combine(path, @"QEngine\EngineAPI\")))
            {
                messageTextBlock.Text = "Unable to find the engine at the specified location";
            }

            if (string.IsNullOrEmpty(messageTextBlock.Text))
            {
                if (!Path.EndsInDirectorySeparator(path)) path += @"\";
                QuietPath = path;
                DialogResult = true;
                Close();
            }
        }
    }
}
