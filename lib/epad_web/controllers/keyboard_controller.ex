defmodule EpadWeb.KeyboardController do
  use EpadWeb, :controller

  plug :put_layout, "keyboard.html"

  def index(conn, _params) do
    render(conn, "index.html")
  end
end
