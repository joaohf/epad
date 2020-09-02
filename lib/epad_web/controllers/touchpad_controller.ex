defmodule EpadWeb.TouchpadController do
  use EpadWeb, :controller

  plug :put_layout, "touchpad.html"

  def index(conn, _params) do
    render(conn, "index.html")
  end
end
